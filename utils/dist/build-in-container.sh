#!/usr/bin/env bash

# build-in-container.sh
#
# Runs inside the build container with the repo mounted at /src; invoked
# by utils/dist/build-linux.sh, not meant for direct host use. Builds
# nmail with static external libs and stages the install tree in
# dist/nmail-linux-<arch>-<libc>/.
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

LIBC="${1:?usage: build-in-container.sh <musl|glibc>}"

ARCH="$(uname -m)"
case "${ARCH}" in
  aarch64) ARCH="arm64" ;;
esac
TARGET="linux-${ARCH}-${LIBC}"

SRC="/src"
BUILD_DIR="${SRC}/build-dist/${TARGET}"
STAGE_DIR="${SRC}/dist/nmail-${TARGET}"

# nmail is pure C/C++ with modest per-TU memory (no tdlib), so build at full
# width by default; override with JOBS.
JOBS="${JOBS:-$(nproc)}"
[[ -z "${JOBS}" ]] && JOBS=1

# musl: a plain -static link yields a fully static, full-featured binary (nmail
# has no Go/cgo runtime, so none of nchat's musl workarounds — the go-patch,
# -no-pie, c-archive bring-up — apply). glibc: point CMake at the source-built
# static deps in /opt/nmail-deps (see Dockerfile.manylinux); only libc stays
# dynamic (HAS_STATIC_EXTLIBS adds -static-libstdc++/-static-libgcc, normal PIE
# dynamic link, so no -static here).
EXTRA_CMAKE_ARGS=()
LINKER_FLAGS=""
if [[ "${LIBC}" == "musl" ]]; then
  # -static: fully static (musl) binary. -no-pie: link a classic non-PIE
  # executable so its code sits at fixed link-time addresses. That lets the
  # musl crash handler's frame-pointer backtrace (src/util.cpp) log absolute
  # addresses that resolve directly with `addr2line -e nmail.debug <addr>`,
  # with no ASLR load-base rebasing. Alpine's toolchain defaults to -pie, which
  # under -static would otherwise yield a static-PIE binary with ASLR-shifted,
  # unresolvable addresses. Pairs with -fno-omit-frame-pointer (CMakeLists.txt).
  LINKER_FLAGS="-static -no-pie"
elif [[ "${LIBC}" == "glibc" ]]; then
  DEPS="/opt/nmail-deps"
  EXTRA_CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="${DEPS}" -DOPENSSL_ROOT_DIR="${DEPS}")
  # Put the static-deps prefix first on the link search path: -L is searched
  # before system dirs and the prefix holds only static archives, so a dep that
  # also ships a dynamic .so under the manylinux base (e.g. curl/xapian/expat in
  # /usr/lib64) still links static as required.
  LINKER_FLAGS="-L${DEPS}/lib"
fi

# Stamp a build-id so the detached nmail.debug can also be matched to the binary
# by hash (and is debuginfod-ready); harmless alongside the .gnu_debuglink path
# that install.sh --debug actually relies on.
LINKER_FLAGS="${LINKER_FLAGS} -Wl,--build-id=sha1"

# The static dependency archives are DWARF-stripped in the build image itself
# (see the "strip --strip-debug" step in Dockerfile.manylinux / Dockerfile.alpine),
# not here: this container runs as a non-root --user that cannot rewrite those
# root-owned archives, so an in-container strip fails "Permission denied" and
# leaves the deps' full -g DWARF to leak into nmail.debug. Stripping them as root
# when the image is built keeps that debug info out of the static link.

# Configure from a clean build dir so dist builds are reproducible and no
# stale CMake cache entry leaks across runs (the repo is bind-mounted, so
# ${BUILD_DIR} otherwise persists). ccache still accelerates recompiles.
# Note: ccache is wired in by the top-level CMakeLists.txt via a global
# RULE_LAUNCH_COMPILE; do NOT also set CMAKE_*_COMPILER_LAUNCHER here or
# ccache 4.x aborts with "Recursive invocation" (ccache ccache <compiler>).
rm -rf "${BUILD_DIR}"
cmake -S "${SRC}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAS_DEBUG_SYMBOLS=ON \
  -DHAS_STATIC_EXTLIBS=ON \
  -DCMAKE_EXE_LINKER_FLAGS="${LINKER_FLAGS}" \
  "${EXTRA_CMAKE_ARGS[@]}" \
  -DCMAKE_INSTALL_PREFIX="/" \
  -DCMAKE_INSTALL_MANDIR="share/man"

cmake --build "${BUILD_DIR}" -j "${JOBS}"

rm -rf "${STAGE_DIR}"
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}"
rm -rf "${STAGE_DIR:?}/lib"
# LICENSE and the combined THIRD_PARTY_LICENSES are installed under
# share/doc/nmail/ by the CMake install rule (added in Phase 8); package.sh
# also surfaces them at the archive top level.

BIN="${STAGE_DIR}/bin/nmail"

# Split debug info out of the release binary: copy the DWARF into a detached
# nmail.debug sibling, strip the shipped binary to its release form (matching the
# former `cmake --install --strip`), then record a .gnu_debuglink so gdb
# auto-loads nmail.debug whenever it sits beside the binary. -g does not change
# codegen, so the stripped binary is byte-for-byte what a plain Release build
# produced. package.sh ships nmail.debug in a separate tarball; install.sh
# --debug drops it back next to the installed binary.
objcopy --only-keep-debug "${BIN}" "${BIN}.debug"
chmod 0644 "${BIN}.debug"
strip --strip-all "${BIN}"
( cd "${STAGE_DIR}/bin" && objcopy --add-gnu-debuglink="nmail.debug" "nmail" )

file "${BIN}"

# glibc target: verify the "mostly static" contract holds — nothing imports
# a glibc newer than 2.28, and only libc/libm/libpthread/libdl/librt/ld-linux
# remain dynamically linked (every third-party lib must be static).
if [[ "${LIBC}" == "glibc" ]]; then
  echo "$0: verifying glibc floor (<= 2.28) and dynamic-link allowlist..."

  BADVER="$(objdump -T "${BIN}" \
    | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' \
    | sed 's/GLIBC_//' | sort -uV \
    | awk -F. '($1 > 2) || ($1 == 2 && $2 > 28)')"
  if [[ -n "${BADVER}" ]]; then
    echo "$0: ERROR: binary imports glibc symbols newer than 2.28:" >&2
    # shellcheck disable=SC2001  # per-line prefix; sed reads clearer than a loop
    echo "${BADVER}" | sed 's/^/  GLIBC_/' >&2
    exit 1
  fi

  # linux-vdso and ld-linux are virtual/loader entries, not real deps.
  ALLOWED='^(libc|libm|libpthread|libdl|librt)\.so'
  UNEXPECTED="$(ldd "${BIN}" 2>/dev/null \
    | grep -oE '\b[a-zA-Z0-9_+-]+\.so[.0-9]*' \
    | grep -vE '^(linux-vdso|ld-linux)' \
    | grep -vE "${ALLOWED}" | sort -u || true)"
  if [[ -n "${UNEXPECTED}" ]]; then
    echo "$0: ERROR: binary dynamically links unexpected libraries:" >&2
    # shellcheck disable=SC2001  # per-line prefix; sed reads clearer than a loop
    echo "${UNEXPECTED}" | sed 's/^/  /' >&2
    exit 1
  fi

  echo "$0: glibc verification passed:"
  ldd "${BIN}" | sed 's/^/  /'
fi
