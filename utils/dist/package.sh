#!/usr/bin/env bash

# package.sh
#
# Assembles distributable tarballs from the staged install trees produced
# by build-linux.sh / build-macos.sh. For each staged tree
# dist/nmail-<target>/ it emits:
#
#   dist/nmail-<version>-<target>.tar.gz   (top dir: nmail-<version>-<target>/)
#
# The tarball carries the whole staged tree: bin/nmail (the oauth2nmail and
# html2nmail helper scripts are bundled inside the binary, extracted at
# runtime), share/man, and LICENSE + THIRD_PARTY_LICENSES.
#
# With NMAIL_DIST_BARE=1 it additionally emits (not published by default —
# the tarball is the complete/recommended artifact):
#
#   dist/nmail-<version>-<target>          (the bare stripped bin/nmail)
#
# The bare binary is a lower-friction, drop-into-a-bin-dir convenience: it
# carries no LICENSE or man page. Every target's binary is full-featured
# (the musl build has no feature caveat, and bundles the helper scripts).
#
# Alongside the artifacts it writes one aggregate checksum file covering
# everything packaged in the run (rewritten each run, `sha256sum -c` format
# with bare filenames so it verifies from within dist/):
#
#   dist/checksums.txt
#
# This mirrors the release layout: the published GitHub release ships a
# single checksums.txt (recreated across every target's tarball) rather than
# a per-asset sidecar.
#
# Version is derived from the git tag at HEAD (leading "v" stripped),
# falling back to NMAIL_VERSION in src/version.cpp (so the tarball version
# matches what `nmail --version` reports). Override with NMAIL_DIST_VERSION.
#
# Usage:
#   utils/dist/package.sh                  package every staged tree in dist/
#   utils/dist/package.sh <target> ...     package only the named target(s)
#                                            e.g. linux-x86_64-glibc, macos-arm64
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
DIST_DIR="${REPO_DIR}/dist"

dist_version() {
  if [[ -n "${NMAIL_DIST_VERSION:-}" ]]; then
    echo "${NMAIL_DIST_VERSION}"
    return
  fi
  # A tag pointing exactly at HEAD marks a release build; use it verbatim
  # (minus a leading "v"). Untagged/dev builds fall back to version.cpp.
  local tag
  tag="$(git -C "${REPO_DIR}" describe --tags --exact-match 2>/dev/null || true)"
  if [[ -n "${tag}" ]]; then
    echo "${tag#v}"
    return
  fi
  awk -F'"' '/#define NMAIL_VERSION/ { print $2; exit }' \
    "${REPO_DIR}/src/version.cpp"
}

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$@"
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$@"
  else
    echo "$0: no sha256 tool (need sha256sum or shasum)" >&2
    return 1
  fi
}

VERSION="$(dist_version)"
if [[ -z "${VERSION}" ]]; then
  echo "$0: unable to determine version" >&2
  exit 1
fi

# Collect target names from args, or auto-detect staged trees in dist/.
targets=("$@")
if [[ ${#targets[@]} -eq 0 ]]; then
  shopt -s nullglob
  for stage in "${DIST_DIR}"/nmail-*/; do
    [[ -f "${stage}bin/nmail" ]] || continue
    name="$(basename "${stage}")"   # nmail-<target>
    targets+=("${name#nmail-}")
  done
  shopt -u nullglob
fi

if [[ ${#targets[@]} -eq 0 ]]; then
  echo "$0: no staged install trees found under dist/ (run build-*.sh first)" >&2
  exit 1
fi

packaged=0
artifacts=()
for target in "${targets[@]}"; do
  stage="${DIST_DIR}/nmail-${target}"
  if [[ ! -f "${stage}/bin/nmail" ]]; then
    echo "$0: skipping ${target}: no staged tree at ${stage}" >&2
    continue
  fi

  pkg="nmail-${VERSION}-${target}"
  tarball="${pkg}.tar.gz"

  # Assemble the versioned top-level dir in a temp area so the staged tree
  # (reused by re-packaging) keeps its unversioned name.
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT
  cp -R "${stage}" "${work}/${pkg}"
  # Surface the license notices at the archive top level for visibility; they
  # are also installed under share/doc/nmail/ by the CMake install rule.
  docdir="${work}/${pkg}/share/doc/nmail"
  for f in LICENSE THIRD_PARTY_LICENSES; do
    [[ -f "${docdir}/${f}" ]] && cp "${docdir}/${f}" "${work}/${pkg}/${f}"
  done
  tar -czf "${DIST_DIR}/${tarball}" -C "${work}" "${pkg}"
  rm -rf "${work}"
  trap - EXIT

  artifacts+=("${tarball}")
  echo "packaged: dist/${tarball}"

  # Optionally also emit the bare stripped binary on its own (opt-in, not
  # part of the default release artifact set).
  if [[ "${NMAIL_DIST_BARE:-0}" == "1" ]]; then
    cp "${stage}/bin/nmail" "${DIST_DIR}/${pkg}"
    chmod 0755 "${DIST_DIR}/${pkg}"
    artifacts+=("${pkg}")
    echo "          dist/${pkg}"
  fi
  packaged=$((packaged + 1))
done

if [[ ${packaged} -eq 0 ]]; then
  echo "$0: nothing packaged" >&2
  exit 1
fi

# One aggregate checksum file over everything packaged in this run, in
# `sha256sum -c` format with bare names (sha256 lists files in the order
# given, so sort for a stable, readable listing). The release recreates the
# same file across all targets' tarballs (see .github/workflows/release.yml).
( cd "${DIST_DIR}" && sha256 "${artifacts[@]}" | sort -k2 > checksums.txt )
echo "checksums: dist/checksums.txt"

echo "$0: packaged ${packaged} artifact(s) for version ${VERSION}"
