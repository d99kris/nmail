#!/usr/bin/env bash

# uninstall.sh
#
# Companion to install.sh: removes an nmail release that was installed with the
# one-line installer (or with matching layout). It deletes only the files the
# installer wrote -- the binary, the man page and the bundled license notices --
# and leaves user configuration and mail cache alone.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/d99kris/nmail/master/utils/uninstall.sh | bash
#
# Env:
#   NMAIL_PREFIX    install prefix to uninstall from; default: ~/.local (mirrors
#                   install.sh). Removes $NMAIL_PREFIX/bin/nmail,
#                   $NMAIL_PREFIX/share/man/man1/nmail.1 and
#                   $NMAIL_PREFIX/share/doc/nmail. If the prefix is not writable,
#                   the uninstaller stops and suggests re-running with sudo.
#   NMAIL_REPO      GitHub owner/name used only in the sudo hint URL below;
#                   default: d99kris/nmail
#
# User configuration and mail cache (~/.config/nmail, or the legacy ~/.nmail)
# are intentionally NOT removed -- purge them manually if you want a clean slate.
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

REPO="${NMAIL_REPO:-d99kris/nmail}"

info() { printf '%s\n' "$*"; }
warn() { printf '%s\n' "warning: $*" >&2; }
err()  { printf '%s\n' "error: $*" >&2; }
die()  { err "$*"; exit 1; }

# --- helpers ---------------------------------------------------------------

# Writable if the directory holding <path>, or its nearest existing ancestor,
# is writable -- removing an entry needs write permission on its parent dir,
# not on the entry itself.
parent_writable() { # <path>
  local d
  d="$(dirname "$1")"
  while [[ ! -e "${d}" ]]; do d="$(dirname "${d}")"; done
  [[ -w "${d}" ]]
}

# --- main ------------------------------------------------------------------

main() {
  local arg
  for arg in "$@"; do
    case "${arg}" in
      -h|--help)
        info "usage: uninstall.sh  (see the header comment for env vars)"
        return 0 ;;
      *) die "unknown argument: ${arg}" ;;
    esac
  done

  local prefix bindir mandir docdir
  if [[ -n "${NMAIL_PREFIX:-}" ]]; then
    prefix="${NMAIL_PREFIX%/}"
  else
    prefix="${HOME}/.local"
  fi
  bindir="${prefix}/bin"
  mandir="${prefix}/share/man"
  docdir="${prefix}/share/doc/nmail"

  # Everything install.sh may have written.
  local candidates=(
    "${bindir}/nmail"
    "${mandir}/man1/nmail.1"
    "${docdir}/LICENSE"
    "${docdir}/THIRD_PARTY_LICENSES"
  )

  local targets=() p
  for p in "${candidates[@]}"; do
    [[ -e "${p}" || -L "${p}" ]] && targets+=("${p}")
  done

  if [[ ${#targets[@]} -eq 0 ]]; then
    info "nothing to uninstall (no installed nmail found under ${prefix})"
    return 0
  fi

  # Gate on writability up front, so we do not remove some files and then stop
  # half-way at the first permission error.
  for p in "${targets[@]}"; do
    if ! parent_writable "${p}"; then
      err "cannot remove ${p}: its directory is not writable by the current user"
      err "re-run with sudo, e.g.:"
      err "  curl -fsSL https://raw.githubusercontent.com/${REPO}/master/utils/uninstall.sh | sudo NMAIL_PREFIX=${prefix} bash"
      die "or set NMAIL_PREFIX to the prefix nmail was installed to (default: ~/.local)"
    fi
  done

  for p in "${targets[@]}"; do
    info "removing ${p}"
    rm -f "${p}"
  done

  # docdir is nmail-specific, so remove it once emptied. rmdir refuses a
  # non-empty dir, so anything the user added there is preserved.
  if [[ -d "${docdir}" ]] && rmdir "${docdir}" 2>/dev/null; then
    info "removing empty ${docdir}"
  fi

  info "successfully uninstalled nmail"
}

main "$@"
