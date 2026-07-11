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
  LINKER_FLAGS="-static"
elif [[ "${LIBC}" == "glibc" ]]; then
  DEPS="/opt/nmail-deps"
  EXTRA_CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="${DEPS}" -DOPENSSL_ROOT_DIR="${DEPS}")
  # Put the static-deps prefix first on the link search path: -L is searched
  # before system dirs and the prefix holds only static archives, so a dep that
  # also ships a dynamic .so under the manylinux base (e.g. curl/xapian/expat in
  # /usr/lib64) still links static as required.
  LINKER_FLAGS="-L${DEPS}/lib"
fi

# Configure from a clean build dir so dist builds are reproducible and no
# stale CMake cache entry leaks across runs (the repo is bind-mounted, so
# ${BUILD_DIR} otherwise persists). ccache still accelerates recompiles.
# Note: ccache is wired in by the top-level CMakeLists.txt via a global
# RULE_LAUNCH_COMPILE; do NOT also set CMAKE_*_COMPILER_LAUNCHER here or
# ccache 4.x aborts with "Recursive invocation" (ccache ccache <compiler>).
rm -rf "${BUILD_DIR}"
cmake -S "${SRC}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAS_STATIC_EXTLIBS=ON \
  -DCMAKE_EXE_LINKER_FLAGS="${LINKER_FLAGS}" \
  "${EXTRA_CMAKE_ARGS[@]}" \
  -DCMAKE_INSTALL_PREFIX="/" \
  -DCMAKE_INSTALL_MANDIR="share/man"

cmake --build "${BUILD_DIR}" -j "${JOBS}"

rm -rf "${STAGE_DIR}"
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}" --strip
rm -rf "${STAGE_DIR:?}/lib"
# LICENSE and the combined THIRD_PARTY_LICENSES are installed under
# share/doc/nmail/ by the CMake install rule (added in Phase 8); package.sh
# also surfaces them at the archive top level.

BIN="${STAGE_DIR}/bin/nmail"
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
