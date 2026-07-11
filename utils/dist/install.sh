#!/usr/bin/env bash

# install.sh
#
# One-line installer for the portable, statically linked nmail release
# binaries. Detects the OS, CPU architecture and (on Linux) libc, picks the
# matching release artifact, verifies its checksum and installs it.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/d99kris/nmail/master/utils/dist/install.sh | bash
#
# Env:
#   NMAIL_VERSION   install a specific release instead of the latest
#                   (accepts "5.14.2" or "v5.14.2"); default: latest release
#   PREFIX          install prefix; default: ~/.local (installs $PREFIX/bin,
#                   $PREFIX/share/man). A non-writable prefix self-elevates
#                   with sudo.
#   NMAIL_REPO      GitHub owner/name to fetch from; default: d99kris/nmail
#
# On glibc Linux the glibc build is selected; on musl Linux the fully static
# musl build; on macOS the arm64 build. Every target's binary is full-featured
# (the musl build has no feature caveat). The full tarball is fetched so the
# man page and helper scripts (oauth2nmail, html2nmail) come along.
#
# The two helper scripts have external interpreter dependencies that are NOT
# bundled: oauth2nmail needs python3 (only for OAuth2 Gmail/Outlook login) and
# html2nmail needs w3m (only for rich HTML-email rendering). Core IMAP mail
# with password authentication works with neither.
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

REPO="${NMAIL_REPO:-d99kris/nmail}"

info() { printf '%s\n' "install.sh: $*" >&2; }
warn() { printf '%s\n' "install.sh: warning: $*" >&2; }
err()  { printf '%s\n' "install.sh: error: $*" >&2; }
die()  { err "$*"; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# --- HTTP helpers (curl preferred, wget fallback) --------------------------

fetch_stdout() { # <url>
  if have curl; then curl -fsSL "$1"
  elif have wget; then wget -qO- "$1"
  else die "need curl or wget to download"; fi
}

fetch_file() { # <url> <dest>
  if have curl; then curl -fsSL -o "$2" "$1"
  elif have wget; then wget -qO "$2" "$1"
  else die "need curl or wget to download"; fi
}

# Resolve the latest release tag (e.g. v5.14.2). Release asset names embed the
# version, so "/releases/latest/download/<asset>" cannot be used blind — the
# version has to be resolved first. The redirect that /releases/latest issues
# to /releases/tag/<tag> gives it without touching the rate-limited API; the
# wget path falls back to the API.
resolve_latest_tag() {
  local eff tag
  if have curl; then
    eff="$(curl -fsSL -o /dev/null -w '%{url_effective}' \
      "https://github.com/${REPO}/releases/latest")" || return 1
    tag="${eff##*/tag/}"
    [[ -n "${tag}" && "${tag}" != "${eff}" ]] || return 1
    printf '%s' "${tag}"
  else
    fetch_stdout "https://api.github.com/repos/${REPO}/releases/latest" \
      | sed -nE 's/.*"tag_name" *: *"([^"]+)".*/\1/p' | head -n1
  fi
}

# --- platform detection ----------------------------------------------------

detect_os() {
  case "$(uname -s)" in
    Linux) echo linux ;;
    Darwin) echo macos ;;
    *) return 1 ;;
  esac
}

detect_arch() {
  # Apple Silicon under a Rosetta shell reports x86_64; trust the CPU flag.
  if [[ "$(uname -s)" == "Darwin" ]] && \
     [[ "$(sysctl -n hw.optional.arm64 2>/dev/null || echo 0)" == "1" ]]; then
    echo arm64; return 0
  fi
  case "$(uname -m)" in
    x86_64|amd64) echo x86_64 ;;
    aarch64|arm64) echo arm64 ;;
    *) return 1 ;;
  esac
}

detect_libc() {
  # ldd --version prints "musl" on musl systems (to stderr) and GNU libc info
  # on glibc. Fall back to the loader path, defaulting to glibc.
  if ldd --version 2>&1 | grep -qi musl; then echo musl; return; fi
  if ls /lib/ld-musl-* >/dev/null 2>&1; then echo musl; return; fi
  echo glibc
}

# --- checksum --------------------------------------------------------------

verify_checksum() { # <dir> <file>  (expects an aggregate checksums.txt alongside)
  local dir="$1" file="$2" line
  # The release ships one checksums.txt covering every target's tarball. Pull
  # just this asset's line so verification does not fail over the other
  # targets' tarballs, which we did not download. $NF is the filename field;
  # strip a leading '*' (sha256sum's binary-mode marker) before matching.
  line="$(awk -v a="${file}" '{ n = $NF; sub(/^\*/, "", n); if (n == a) print }' \
    "${dir}/checksums.txt")"
  [[ -n "${line}" ]] || die "no checksum entry for ${file} in checksums.txt"
  if have sha256sum; then
    ( cd "${dir}" && printf '%s\n' "${line}" | sha256sum -c - >/dev/null )
  elif have shasum; then
    ( cd "${dir}" && printf '%s\n' "${line}" | shasum -a 256 -c - >/dev/null )
  else
    die "no sha256 tool available (need sha256sum or shasum) to verify the download"
  fi
}

# --- privileged install helpers --------------------------------------------

# Writable if the target dir, or its nearest existing ancestor, is writable.
target_writable() { # <dir>
  local d="$1"
  while [[ ! -e "${d}" ]]; do d="$(dirname "${d}")"; done
  [[ -w "${d}" ]]
}

SUDO=""
run_priv() {
  if [[ -n "${SUDO}" ]]; then sudo "$@"; else "$@"; fi
}

# --- main ------------------------------------------------------------------

main() {
  local os arch libc target version tag
  os="$(detect_os)" || die "unsupported OS: $(uname -s)"
  arch="$(detect_arch)" || die "unsupported architecture: $(uname -m)"

  case "${os}" in
    macos)
      [[ "${arch}" == "arm64" ]] || \
        die "macOS builds are Apple Silicon (arm64) only; Intel is not supported"
      target="macos-arm64"
      ;;
    linux)
      libc="$(detect_libc)"
      target="linux-${arch}-${libc}"
      ;;
  esac

  if [[ -n "${NMAIL_VERSION:-}" ]]; then
    version="${NMAIL_VERSION#v}"
    tag="v${version}"
    info "installing pinned version ${version} (${target})"
  else
    tag="$(resolve_latest_tag)" || die "could not resolve the latest release for ${REPO}"
    version="${tag#v}"
    info "latest release is ${tag} (${target})"
  fi

  local asset base srcdir
  asset="nmail-${version}-${target}.tar.gz"
  base="https://github.com/${REPO}/releases/download/${tag}"

  # tmp is intentionally NOT local: the EXIT trap fires after main() returns, so
  # a function-local would be out of scope by then — under `set -u` that both
  # errors ("tmp: unbound variable", nonzero exit on an otherwise-successful
  # install) and skips the cleanup. A script-scope var keeps the trap working.
  tmp="$(mktemp -d)"
  trap 'rm -rf "${tmp}"' EXIT

  info "downloading ${asset}"
  fetch_file "${base}/${asset}" "${tmp}/${asset}" \
    || die "download failed (no ${asset} in release ${tag}?): ${base}/${asset}"
  fetch_file "${base}/checksums.txt" "${tmp}/checksums.txt" \
    || die "download failed: ${base}/checksums.txt"

  info "verifying checksum"
  verify_checksum "${tmp}" "${asset}" || die "checksum verification failed for ${asset}"

  tar -xzf "${tmp}/${asset}" -C "${tmp}"
  srcdir="${tmp}/nmail-${version}-${target}"
  [[ -x "${srcdir}/bin/nmail" ]] || die "unexpected archive layout: ${srcdir}/bin/nmail missing"

  local prefix bindir mandir
  if [[ -n "${PREFIX:-}" ]]; then
    prefix="${PREFIX%/}"
  else
    prefix="${HOME}/.local"
  fi
  bindir="${prefix}/bin"
  mandir="${prefix}/share/man"

  if ! target_writable "${prefix}"; then
    if have sudo; then
      SUDO="sudo"
      info "${prefix} is not writable; using sudo"
    else
      die "${prefix} is not writable and sudo is unavailable; set PREFIX to a writable location"
    fi
  fi

  info "installing to ${bindir}/nmail"
  run_priv mkdir -p "${bindir}" "${mandir}/man1"
  run_priv cp "${srcdir}/bin/nmail" "${bindir}/nmail"
  run_priv chmod 0755 "${bindir}/nmail"

  # Optional helper scripts (ship in bin/ alongside nmail). They carry external
  # interpreter deps that are not bundled: oauth2nmail -> python3 (OAuth2),
  # html2nmail -> w3m (HTML rendering). Install whichever the tarball provides.
  local helper
  for helper in oauth2nmail html2nmail; do
    if [[ -f "${srcdir}/bin/${helper}" ]]; then
      run_priv cp "${srcdir}/bin/${helper}" "${bindir}/${helper}"
      run_priv chmod 0755 "${bindir}/${helper}"
    fi
  done

  if [[ -f "${srcdir}/share/man/man1/nmail.1" ]]; then
    run_priv cp "${srcdir}/share/man/man1/nmail.1" "${mandir}/man1/nmail.1"
    run_priv chmod 0644 "${mandir}/man1/nmail.1"
  fi

  # A curl/tar install sets no quarantine attribute and the binary is already
  # ad-hoc signed, so this is only a defensive belt-and-braces for the
  # browser-downloaded-then-unpacked edge case.
  if [[ "${os}" == "macos" ]] && have xattr; then
    run_priv xattr -dr com.apple.quarantine "${bindir}/nmail" 2>/dev/null || true
  fi

  info "installed nmail ${version} to ${bindir}/nmail"
  info "note: OAuth2 login (oauth2nmail) needs python3; HTML rendering (html2nmail) needs w3m;"
  info "      core IMAP mail with password authentication needs neither"

  case ":${PATH}:" in
    *":${bindir}:"*) ;;
    *)
      info "note: ${bindir} is not on your PATH — add it, e.g.:"
      info "  export PATH=\"${bindir}:\$PATH\""
      ;;
  esac
}

main "$@"
