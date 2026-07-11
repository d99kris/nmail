#!/usr/bin/env bash

# build-deps-macos.sh
#
# Builds pinned static third-party libraries for the macOS dist build into
# a cached prefix (~/.cache/nmail-dist/deps-macos-arm64, override the base
# dir with NMAIL_DIST_CACHE). Sources are built from upstream release
# tarballs rather than homebrew kegs so anyone can reproduce the dist
# artifacts without a homebrew state dependency. Versions are kept in sync
# with utils/dist/Dockerfile.manylinux so all targets carry identical dep
# versions and auth/protocol behaviour.
#
# nmail is pure C/C++, so this is a subset of the Linux dependency set:
#   - curl, expat and iconv stay dynamic (stable macOS system dylibs under
#     /usr/lib, allowed by the build-macos.sh otool allowlist), so they are
#     NOT built here.
#   - libuuid is not built: uuid_generate/uuid_unparse live in libSystem and
#     <uuid/uuid.h> ships in the SDK (see the top-level CMakeLists).
# Everything version-sensitive / non-system is source-built static below:
# zlib, openssl, bzip2/xz/zstd (libmagic backends), libmagic, ncursesw,
# sqlite3, xapian-core and cyrus-sasl.
#
# Each package is skipped if its version stamp is already present in the
# prefix; remove the prefix to rebuild from scratch. Invoked automatically
# by utils/dist/build-macos.sh.
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]] || [[ "$(uname -m)" != "arm64" ]]; then
  echo "$0: must run on a macOS arm64 host" >&2
  exit 1
fi

# Deployment target for all dep builds; must match build-macos.sh so the
# final link does not mix objects with different minimum OS versions.
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"

CACHE_DIR="${NMAIL_DIST_CACHE:-${HOME}/.cache/nmail-dist}"
DEPS="${CACHE_DIR}/deps-macos-arm64"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

# Let configure scripts pick up already-built deps (zlib for libmagic and
# xapian, etc.) from the prefix.
export CPPFLAGS="-I${DEPS}/include"
export LDFLAGS="-L${DEPS}/lib"
export PKG_CONFIG_PATH="${DEPS}/lib/pkgconfig"

mkdir -p "${DEPS}"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

# built <pkg>-<version>: true if the package stamp exists (skip build)
built() {
  if [[ -f "${DEPS}/.built-${1}" ]]; then
    echo "$(basename "$0"): ${1} already built, skipping"
    return 0
  fi
  echo "$(basename "$0"): building ${1}"
  return 1
}

# stamp <pkg>-<version>: mark package as built
stamp() {
  touch "${DEPS}/.built-${1}"
}

# fetch <url>: download and extract a release tarball in ${WORK}
fetch() {
  local url="$1" tarball="${1##*/}"
  curl -fsSL --retry 3 --retry-delay 2 "${url}" -o "${tarball}"
  tar xf "${tarball}"
}

# zlib: needed by libmagic and xapian. Fetched from the madler/zlib GitHub
# release (byte-identical to upstream) rather than zlib.net/fossils/, whose host
# intermittently rejects CI/datacenter requests with HTTP 415.
ZLIB_VERSION=1.3.1
if ! built "zlib-${ZLIB_VERSION}"; then
  fetch "https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz"
  ( cd "zlib-${ZLIB_VERSION}" &&
    ./configure --prefix="${DEPS}" --static &&
    make -j"${JOBS}" && make install )
  stamp "zlib-${ZLIB_VERSION}"
fi

# openssl: nmail crypto, libetpan TLS (OPENSSL_USE_STATIC_LIBS). --libdir=lib
# keeps it out of lib64 so a single -L covers everything.
OPENSSL_VERSION=3.0.16
if ! built "openssl-${OPENSSL_VERSION}"; then
  fetch "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
  ( cd "openssl-${OPENSSL_VERSION}" &&
    ./Configure --prefix="${DEPS}" --libdir=lib no-shared no-tests &&
    make -j"${JOBS}" && make install_sw )
  stamp "openssl-${OPENSSL_VERSION}"
fi

# bzip2 / xz / zstd: libmagic's decompression backends. Built before libmagic
# so its configure detects them, and appended after libmagic on nmail's link
# line by the top-level CMakeLists HAS_STATIC_EXTLIBS block.
BZIP2_VERSION=1.0.8
if ! built "bzip2-${BZIP2_VERSION}"; then
  fetch "https://sourceware.org/pub/bzip2/bzip2-${BZIP2_VERSION}.tar.gz"
  ( cd "bzip2-${BZIP2_VERSION}" &&
    make -j"${JOBS}" libbz2.a && make install PREFIX="${DEPS}" )
  stamp "bzip2-${BZIP2_VERSION}"
fi

XZ_VERSION=5.4.7
if ! built "xz-${XZ_VERSION}"; then
  fetch "https://github.com/tukaani-project/xz/releases/download/v${XZ_VERSION}/xz-${XZ_VERSION}.tar.gz"
  ( cd "xz-${XZ_VERSION}" &&
    ./configure --prefix="${DEPS}" --disable-shared --enable-static \
      --disable-xzdec --disable-lzmadec --disable-lzmainfo \
      --disable-scripts --disable-doc &&
    make -j"${JOBS}" && make install )
  stamp "xz-${XZ_VERSION}"
fi

ZSTD_VERSION=1.5.6
if ! built "zstd-${ZSTD_VERSION}"; then
  fetch "https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz"
  ( cd "zstd-${ZSTD_VERSION}" &&
    make -j"${JOBS}" -C lib install-static install-includes install-pc \
      PREFIX="${DEPS}" )
  stamp "zstd-${ZSTD_VERSION}"
fi

# libmagic (file): attachment mime/type detection in src/. Also ships
# magic.mgc (loaded at runtime via magic_load(cookie, NULL)).
FILE_VERSION=5.46
if ! built "file-${FILE_VERSION}"; then
  fetch "https://astron.com/pub/file/file-${FILE_VERSION}.tar.gz"
  ( cd "file-${FILE_VERSION}" &&
    ./configure --prefix="${DEPS}" --disable-shared --enable-static \
      --disable-libseccomp &&
    make -j"${JOBS}" && make install )
  stamp "file-${FILE_VERSION}"
fi

# ncursesw (+ tinfow). Unlike the Linux builds, no compiled-in terminfo
# fallbacks: /usr/share/terminfo is part of macOS so they are not needed,
# and generating them requires the ancient system tic, which cannot compile
# the modern terminfo.src. AWK is pinned to the system awk: config.status
# runs awk with the locale scrubbed (LC_ALL=C), and homebrew gawk then
# silently emits nothing from mk-1st.awk, leaving the generated Makefiles
# without library rules ("No rule to make target '../lib/libtinfow.a'").
NCURSES_VERSION=6.5
if ! built "ncurses-${NCURSES_VERSION}"; then
  fetch "https://ftp.gnu.org/gnu/ncurses/ncurses-${NCURSES_VERSION}.tar.gz"
  ( cd "ncurses-${NCURSES_VERSION}" &&
    AWK=/usr/bin/awk ./configure --prefix="${DEPS}" \
      --without-shared --without-debug --without-ada --without-tests \
      --without-manpages --enable-widec --with-termlib --enable-pc-files \
      --with-pkg-config-libdir="${DEPS}/lib/pkgconfig" \
      --with-default-terminfo-dir=/usr/share/terminfo \
      --with-terminfo-dirs="/usr/share/terminfo:/etc/terminfo" \
      --disable-db-install &&
    make -j"${JOBS}" && make install )
  stamp "ncurses-${NCURSES_VERSION}"
fi

# sqlite3: local message cache backend in src/.
SQLITE_URL=https://www.sqlite.org/2024/sqlite-autoconf-3470200.tar.gz
SQLITE_TARBALL="${SQLITE_URL##*/}"
SQLITE_NAME="${SQLITE_TARBALL%.tar.gz}"
if ! built "${SQLITE_NAME}"; then
  fetch "${SQLITE_URL}"
  ( cd "${SQLITE_NAME}" &&
    ./configure --prefix="${DEPS}" --disable-shared --enable-static &&
    make -j"${JOBS}" && make install )
  stamp "${SQLITE_NAME}"
fi

# xapian-core: full-text search index (find_package(Xapian)). Installs its
# CMake package config under ${DEPS}/lib/cmake/xapian so nmail resolves it
# with CMAKE_PREFIX_PATH=${DEPS}. Static libxapian needs zlib after it on the
# link line (handled in the top-level CMakeLists HAS_STATIC_EXTLIBS block).
XAPIAN_VERSION=1.4.27
if ! built "xapian-core-${XAPIAN_VERSION}"; then
  fetch "https://oligarchy.co.uk/xapian/${XAPIAN_VERSION}/xapian-core-${XAPIAN_VERSION}.tar.xz"
  ( cd "xapian-core-${XAPIAN_VERSION}" &&
    ./configure --prefix="${DEPS}" --disable-shared --enable-static \
      --disable-documentation &&
    make -j"${JOBS}" && make install )
  stamp "xapian-core-${XAPIAN_VERSION}"
fi

# cyrus-sasl (libsasl2): SMTP password auth goes through libetpan's SASL path;
# nmail also enumerates the client mechanisms at startup via
# sasl_global_listmech() (src/sasl.cpp). In a normal build the mechanisms are
# dlopen'd from */sasl2/*.so — impossible in a static binary — so with
# --enable-static --disable-shared cyrus-sasl bakes the mechanism objects
# straight into libsasl2.a and sasl_client_init() auto-registers them via its
# internal _sasl_static_plugins[] table (no app-side sasl_client_add_plugin
# needed; the dlopen path is compiled out). IMAP auth (password + OAuth2) and
# SMTP OAuth2 do NOT use cyrus-sasl (libetpan drives those directly), so this
# only gates SMTP password login.
#
# We compile in PLAIN + LOGIN + CRAM-MD5 (LOGIN is OFF by default and must be
# enabled; CRAM-MD5 uses cyrus-sasl's own internal MD5, no libcrypto). We
# deliberately DISABLE DIGEST-MD5 and SCRAM: they are the only client
# mechanisms that reference OpenSSL (DES/RC4, digests), which would make
# libsasl2.a depend on libcrypto appearing AFTER it on the single-pass link
# line (nmail lists libcrypto before libsasl2). The result is a self-contained
# libsasl2.a (needs only libc), and PLAIN/LOGIN/CRAM-MD5 cover realistic SMTP
# password auth (DIGEST-MD5 is obsolete).
#
# CFLAGS mirror the Linux Dockerfiles: cyrus-sasl 2.1.28 (2021) predates the
# compiler defaults that break it — -Wimplicit-function-declaration /
# -Wint-conversion promoted to errors (lib/saslutil.c calls time()/clock()
# without <time.h>; benign) and C23 dropping K&R definitions (common/md5.c is
# K&R). -std=gnu17 plus the two -Wno flags keep the mature C compiling under
# modern clang as well.
#
# macOS deviation: --disable-macos-framework. On Darwin cyrus-sasl defaults to
# building a replacement SASL2.framework and hard-installs its headers into
# /Library/Frameworks (root-only, and not the plain static libsasl2.a we want);
# disabling it makes `make install` a normal prefix install into ${DEPS}.
CYRUS_SASL_VERSION=2.1.28
if ! built "cyrus-sasl-${CYRUS_SASL_VERSION}"; then
  fetch "https://github.com/cyrusimap/cyrus-sasl/releases/download/cyrus-sasl-${CYRUS_SASL_VERSION}/cyrus-sasl-${CYRUS_SASL_VERSION}.tar.gz"
  ( cd "cyrus-sasl-${CYRUS_SASL_VERSION}" &&
    ./configure --prefix="${DEPS}" --disable-shared --enable-static \
      CFLAGS="-O2 -std=gnu17 -Wno-implicit-function-declaration -Wno-int-conversion" \
      --with-dblib=none --disable-checkapop --disable-sample \
      --disable-macos-framework \
      --enable-plain --enable-login --enable-cram \
      --disable-digest --disable-scram --disable-gssapi --disable-krb4 \
      --disable-otp --disable-srp --disable-ntlm --disable-anon &&
    make -j"${JOBS}" && make install )
  ar t "${DEPS}/lib/libsasl2.a" | grep -qE '^plain\.o$' ||
    { echo "$0: libsasl2.a missing baked-in plain plugin" >&2; exit 1; }
  stamp "cyrus-sasl-${CYRUS_SASL_VERSION}"
fi

echo "$(basename "$0"): all deps present in ${DEPS}"
