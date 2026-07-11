#!/usr/bin/env bash

# build-macos.sh
#
# Builds a portable nmail macOS arm64 binary and stages the install tree in
# dist/nmail-macos-arm64/. Third-party libraries are linked static from a
# source-built deps prefix (utils/dist/build-deps-macos.sh, invoked
# automatically; cached under ~/.cache/nmail-dist/); only stable system
# libraries (curl, expat, iconv, libc++, libSystem) and frameworks stay
# dynamic, with a macOS 12.0 deployment floor. The binary is ad-hoc
# codesigned — installs via curl/tar run as-is, browser-downloaded archives
# need the quarantine attribute cleared (xattr -d com.apple.quarantine) or
# right-click-open.
#
# Usage:
#   utils/dist/build-macos.sh
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]] || [[ "$(uname -m)" != "arm64" ]]; then
  echo "$0: must run on a macOS arm64 host" >&2
  exit 1
fi

# Exported so build-deps-macos.sh (below) targets the same minimum OS version;
# also passed to cmake as CMAKE_OSX_DEPLOYMENT_TARGET, so the deps and nmail
# agree and the link does not mix objects with different floors.
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"

TARGET="macos-arm64"
REPO_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CACHE_DIR="${NMAIL_DIST_CACHE:-${HOME}/.cache/nmail-dist}"
DEPS="${CACHE_DIR}/deps-${TARGET}"
BUILD_DIR="${REPO_DIR}/build-dist/${TARGET}"
STAGE_DIR="${REPO_DIR}/dist/nmail-${TARGET}"

"${REPO_DIR}/utils/dist/build-deps-macos.sh"

# The static deps are built by autotools with debug info (autotools defaults
# CFLAGS/CXXFLAGS to "-g -O2"), so their .a archives carry DWARF for hundreds of
# TUs -- xapian alone is ~200. dsymutil would otherwise harvest all of it into
# nmail.dSYM via the debug map, dwarfing nmail's own ~34 TUs (measured: ~260 of
# 294 compile units in the symbols came from deps). strip -S drops the debug
# map/DWARF but keeps the external symbol table, so the static link is
# unaffected and only nmail's own frames end up in the detached symbols.
# Idempotent; runs against the cached prefix each build.
find "${DEPS}/lib" -name '*.a' -exec strip -S {} + 2>/dev/null || true

JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

GENERATOR=()
if command -v ninja &> /dev/null; then
  GENERATOR=(-G Ninja)
fi

# Configure from a clean build dir so dist builds are reproducible and no stale
# CMake cache entry leaks across runs; ccache (if installed) still accelerates
# recompiles. CMAKE_PREFIX_PATH points dependency detection (and the in-tree
# ext/libetpan subbuild) at the static deps prefix; the *_ROOT_DIR hints steer
# the Darwin homebrew/macports probes in CMakeLists.txt there too.
rm -rf "${BUILD_DIR}"
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" "${GENERATOR[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAS_DEBUG_SYMBOLS=ON \
  -DHAS_STATIC_EXTLIBS=ON \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
  -DCMAKE_PREFIX_PATH="${DEPS}" \
  -DNCURSES_ROOT_DIR="${DEPS}" \
  -DOPENSSL_ROOT_DIR="${DEPS}" \
  -DSQLITE_ROOT_DIR="${DEPS}" \
  -DCMAKE_INSTALL_PREFIX="/" \
  -DCMAKE_INSTALL_MANDIR="share/man"

cmake --build "${BUILD_DIR}" -j "${JOBS}"

rm -rf "${STAGE_DIR}"
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}"
# nmail installs no libraries; the rm keeps parity with the Linux path and is a
# harmless no-op if the directory is absent.
rm -rf "${STAGE_DIR:?}/lib"

BIN="${STAGE_DIR}/bin/nmail"

# Collect DWARF into a detached .dSYM bundle before stripping. dsymutil reads the
# debug map in the binary plus the .o files still present in BUILD_DIR, and the
# bundle is matched to the binary by LC_UUID (which strip -x preserves). -g does
# not change codegen, so the stripped binary is what a plain Release build
# produced. package.sh ships the .dSYM in a separate tarball; install.sh --debug
# drops it back next to the installed binary (use lldb on macOS — gdb is
# unsupported on Apple Silicon).
dsymutil "${BIN}" -o "${BIN}.dSYM"

# Strip local symbols, then re-sign: the CMake install rule ad-hoc signs the
# binary (core-dump entitlement), and stripping invalidates that signature.
strip -x "${BIN}"
"${REPO_DIR}/utils/sign" "${REPO_DIR}/src/nmail.entitlements" "${BIN}"

file "${BIN}"

# Verify the "mostly static" contract holds: only system libraries and
# frameworks dynamic (no homebrew/macports/deps-prefix leakage).
echo "$0: verifying dynamic-link allowlist and deployment target..."

UNEXPECTED="$(otool -L "${BIN}" | tail -n +2 | awk '{print $1}' \
  | grep -vE '^(/usr/lib/|/System/Library/Frameworks/)' | sort -u || true)"
if [[ -n "${UNEXPECTED}" ]]; then
  echo "$0: ERROR: binary dynamically links unexpected libraries:" >&2
  # shellcheck disable=SC2001  # per-line prefix; sed reads clearer than a loop
  echo "${UNEXPECTED}" | sed 's/^/  /' >&2
  exit 1
fi

MINOS="$(otool -l "${BIN}" | awk '/LC_BUILD_VERSION/ { f = 1 } f && /minos/ { print $2; exit }')"
if [[ "${MINOS}" != "${MACOSX_DEPLOYMENT_TARGET}" ]]; then
  echo "$0: ERROR: binary minos ${MINOS} does not match deployment target ${MACOSX_DEPLOYMENT_TARGET}" >&2
  exit 1
fi

if ! codesign --verify "${BIN}"; then
  echo "$0: ERROR: codesign verification failed" >&2
  exit 1
fi

echo "$0: macOS verification passed (minos ${MINOS}):"
otool -L "${BIN}" | tail -n +2 | sed 's/^[[:space:]]*/  /'

echo "artifact: dist/nmail-${TARGET}/bin/nmail"
