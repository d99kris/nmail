#!/usr/bin/env bash

# smoke.sh
#
# Smoke-tests packaged nmail tarballs (produced by package.sh) to catch the
# class of bugs static builds exist to avoid: an unresolved dynamic symbol,
# a too-new glibc, a missing shared library, or a broken terminfo lookup.
#
# For each Linux tarball it extracts the tree once, then in a matrix of clean
# distro containers runs three probes:
#
#   core (fatal)     `nmail --version` and `nmail --help` exit 0 — catches an
#                    unresolved dynamic symbol, a too-new glibc, a missing
#                    shared library.
#   cfg  (non-fatal) launched with an empty HOME and no accounts, nmail writes a
#                    default main.conf, fails config validation on the empty
#                    user, and prints "error: user not specified in config file
#                    (...)" — a full clean startup (config write + crypto init +
#                    validation) without interactive input. nmail exits here,
#                    before ncurses init(), so this does NOT exercise terminfo.
#   tui  (non-fatal) `nmail -k` (key-code dump) reaches initscr() directly, with
#                    no config/network/auth, and prints "key code dump mode ..."
#                    — the genuine ncurses/terminfo probe. If terminfo cannot be
#                    resolved, ncurses instead prints "Error opening terminal"
#                    (source-built ncurses) or "cannot initialize terminal type"
#                    (system ncurses) and exits. It is non-fatal because a
#                    minimal image may ship no terminfo db and the compiled-in
#                    fallbacks only cover common TERM values; a WARN flags an
#                    image whose terminfo the binary could not satisfy.
#
# Only artifacts whose architecture matches the host are tested (no
# emulation); run this on a host of each target arch. Containers require
# docker; a glibc artifact skips the musl-only alpine image.
#
# Usage:
#   utils/dist/smoke.sh                     smoke-test every dist/*.tar.gz
#   utils/dist/smoke.sh <tarball|target>... only the named artifact(s)
#                                             (target e.g. linux-x86_64-glibc)
#
# Env:
#   NMAIL_SMOKE_CONTAINERS  space-separated docker images (override matrix)
#   NMAIL_SMOKE_TERM        TERM for the cfg/tui probes (default xterm-256color)
#   NMAIL_SMOKE_CFG=0       skip the no-config startup probe
#   NMAIL_SMOKE_TUI=0       skip the ncurses/terminfo (-k) probe
#
# nmail is distributed under the MIT license, see LICENSE for details.

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
DIST_DIR="${REPO_DIR}/dist"

# Known packaging targets, longest-suffix match first so the version prefix
# (which itself may contain dashes) never confuses target extraction.
KNOWN_TARGETS=(
  linux-x86_64-musl linux-arm64-musl
  linux-x86_64-glibc linux-arm64-glibc
  macos-arm64
)

DEFAULT_CONTAINERS=(ubuntu:20.04 ubuntu:24.04 debian:11 fedora:latest rockylinux:8 alpine:latest)
if [[ -n "${NMAIL_SMOKE_CONTAINERS:-}" ]]; then
  read -r -a LINUX_CONTAINERS <<< "${NMAIL_SMOKE_CONTAINERS}"
else
  LINUX_CONTAINERS=("${DEFAULT_CONTAINERS[@]}")
fi
SMOKE_TERM="${NMAIL_SMOKE_TERM:-xterm-256color}"
CFG_PROBE="${NMAIL_SMOKE_CFG:-1}"
TUI_PROBE="${NMAIL_SMOKE_TUI:-1}"

# The no-config sentinel: nmail writes a default main.conf then rejects the
# empty user before starting the TUI (src/main.cpp ValidateConfig ->
# ReportConfigError).
SENTINEL="error: user not specified in config file"

# The key-code dump banner, printed after initscr() succeeds (src/main.cpp
# KeyDump). Its presence means ncurses/terminfo initialised.
TUI_SENTINEL="key code dump mode"

norm_arch() {
  case "$1" in
    x86_64|amd64) echo x86_64 ;;
    aarch64|arm64) echo arm64 ;;
    *) echo "$1" ;;
  esac
}

HOST_OS="$(uname -s)"
HOST_ARCH="$(norm_arch "$(uname -m)")"

# target_of <tarball-basename> -> known target suffix on stdout (empty if
# none). Always exits 0 so `target=$(target_of ...)` is safe under set -e.
target_of() {
  local base="${1%.tar.gz}"
  local t
  for t in "${KNOWN_TARGETS[@]}"; do
    [[ "${base}" == *"-${t}" ]] && { echo "${t}"; return 0; }
  done
  return 0
}

# with_timeout <secs> <cmd...> — bounds a probe if a timeout tool exists.
with_timeout() {
  local secs="$1"; shift
  if command -v timeout >/dev/null 2>&1; then
    timeout "${secs}" "$@"
  elif command -v gtimeout >/dev/null 2>&1; then
    gtimeout "${secs}" "$@"
  else
    "$@"
  fi
}

# Resolve args to tarball paths (accept a path or a bare target name).
resolve_args() {
  local arg base match
  if [[ $# -eq 0 ]]; then
    shopt -s nullglob
    for f in "${DIST_DIR}"/nmail-*.tar.gz; do echo "${f}"; done
    shopt -u nullglob
    return
  fi
  for arg in "$@"; do
    if [[ -f "${arg}" ]]; then
      echo "${arg}"; continue
    fi
    # bare target name -> match a tarball in dist/
    match=""
    shopt -s nullglob
    for f in "${DIST_DIR}"/nmail-*-"${arg}".tar.gz; do match="${f}"; done
    shopt -u nullglob
    if [[ -n "${match}" ]]; then
      echo "${match}"
    else
      echo "$0: no tarball for '${arg}' under dist/" >&2
    fi
  done
}

PASS=0; FAIL=0; SKIP=0
declare -a SUMMARY=()

record() { SUMMARY+=("$1"); }

# run_core_linux <image> <platform> <pkg-root> — fatal --version/--help.
# pkg-root is the package top dir (holds bin/); mounted read-only at /opt/nmail.
run_core_linux() {
  local image="$1" platform="$2" root="$3" out rc
  out="$(docker run --rm --platform "${platform}" \
    -v "${root}:/opt/nmail:ro" "${image}" /bin/sh -c '
      set -e
      B=/opt/nmail/bin/nmail
      "$B" --version
      "$B" --help >/dev/null
    ' 2>&1)" && rc=0 || rc=$?
  if [[ ${rc} -eq 0 && "${out}" == *nmail* ]]; then
    return 0
  fi
  # shellcheck disable=SC2001  # per-line prefix; sed reads clearer than a loop
  echo "${out}" | sed 's/^/      /'
  return 1
}

# run_cfg_linux <image> <platform> <pkg-root> — non-fatal no-config startup
# probe (writes main.conf, inits crypto, validates config; exits before ncurses).
run_cfg_linux() {
  local image="$1" platform="$2" root="$3" out
  out="$(with_timeout 30 docker run --rm --platform "${platform}" \
    -e HOME=/tmp/h -e "TERM=${SMOKE_TERM}" \
    -v "${root}:/opt/nmail:ro" "${image}" /bin/sh -c '
      mkdir -p /tmp/h
      /opt/nmail/bin/nmail </dev/null 2>&1
    ' 2>&1)" || true
  [[ "${out}" == *"${SENTINEL}"* ]]
}

# run_terminfo_linux <image> <platform> <pkg-root> — non-fatal ncurses/terminfo
# probe. `nmail -k` reaches initscr() with no config/network/auth; feeding 'q'
# exits the key loop cleanly once ncurses is up (the timeout is the backstop if
# terminfo resolution hangs or the binary busy-loops on stdin EOF).
run_terminfo_linux() {
  local image="$1" platform="$2" root="$3" out
  out="$(with_timeout 20 docker run --rm --platform "${platform}" \
    -e HOME=/tmp/h -e "TERM=${SMOKE_TERM}" \
    -v "${root}:/opt/nmail:ro" "${image}" /bin/sh -c '
      mkdir -p /tmp/h
      printf "q\n" | /opt/nmail/bin/nmail -k 2>&1
    ' 2>&1)" || true
  [[ "${out}" == *"${TUI_SENTINEL}"* ]]
}

smoke_linux() {
  local tarball="$1" target="$2" arch="$3"
  local platform
  case "${arch}" in
    x86_64) platform="linux/amd64" ;;
    arm64) platform="linux/arm64" ;;
  esac

  if ! command -v docker >/dev/null 2>&1; then
    echo "  SKIP ${target}: docker not found (needed for Linux smoke tests)"
    record "SKIP ${target} (no docker)"; SKIP=$((SKIP + 1)); return
  fi

  local tree
  tree="$(mktemp -d)"
  if ! tar -xzf "${tarball}" -C "${tree}" 2>/dev/null; then
    echo "  FAIL ${target}: cannot extract tarball"
    record "FAIL ${target} (bad tarball)"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  fi
  local root
  root="$(dirname "$(dirname "$(echo "${tree}"/*/bin/nmail)")")"
  if [[ ! -x "${root}/bin/nmail" ]]; then
    echo "  FAIL ${target}: no bin/nmail in tarball"
    record "FAIL ${target} (bad tarball)"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  fi

  local image is_musl
  [[ "${target}" == *-musl ]] && is_musl=1 || is_musl=0
  for image in "${LINUX_CONTAINERS[@]}"; do
    # A glibc build cannot run on musl-only alpine; a musl static build runs
    # everywhere. Skip the impossible combo rather than reporting a failure.
    if [[ ${is_musl} -eq 0 && "${image}" == alpine* ]]; then
      echo "  SKIP ${target} @ ${image}: glibc build, alpine has no glibc"
      record "SKIP ${target} @ ${image} (glibc/alpine)"; SKIP=$((SKIP + 1))
      continue
    fi

    if run_core_linux "${image}" "${platform}" "${root}"; then
      local cfg="n/a" tui="n/a"
      if [[ "${CFG_PROBE}" != "0" ]]; then
        run_cfg_linux "${image}" "${platform}" "${root}" && cfg="cfg:ok" || cfg="cfg:WARN"
      fi
      if [[ "${TUI_PROBE}" != "0" ]]; then
        run_terminfo_linux "${image}" "${platform}" "${root}" && tui="tui:ok" || tui="tui:WARN"
      fi
      echo "  PASS ${target} @ ${image}  (${cfg} ${tui})"
      record "PASS ${target} @ ${image} ${cfg} ${tui}"; PASS=$((PASS + 1))
    else
      echo "  FAIL ${target} @ ${image}"
      record "FAIL ${target} @ ${image}"; FAIL=$((FAIL + 1))
    fi
  done

  rm -rf "${tree}"
}

smoke_macos() {
  local tarball="$1" target="$2"
  local tree root out rc
  tree="$(mktemp -d)"
  if ! tar -xzf "${tarball}" -C "${tree}" 2>/dev/null; then
    echo "  FAIL ${target}: cannot extract tarball"
    record "FAIL ${target} (bad tarball)"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  fi
  root="$(dirname "$(dirname "$(echo "${tree}"/*/bin/nmail)")")"
  if [[ ! -x "${root}/bin/nmail" ]]; then
    echo "  FAIL ${target}: no bin/nmail in tarball"
    record "FAIL ${target} (bad tarball)"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  fi

  out="$("${root}/bin/nmail" --version 2>&1)" && rc=0 || rc=$?
  if [[ ${rc} -ne 0 || "${out}" != *nmail* ]]; then
    # shellcheck disable=SC2001  # per-line prefix; sed reads clearer than a loop
    echo "${out}" | sed 's/^/      /'
    echo "  FAIL ${target} @ host (--version)"
    record "FAIL ${target} @ host"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  fi
  "${root}/bin/nmail" --help >/dev/null 2>&1 || {
    echo "  FAIL ${target} @ host (--help)"
    record "FAIL ${target} @ host"; FAIL=$((FAIL + 1))
    rm -rf "${tree}"; return
  }

  local cfg="n/a" tui="n/a" home
  if [[ "${CFG_PROBE}" != "0" ]]; then
    home="$(mktemp -d)"
    out="$(HOME="${home}" TERM="${SMOKE_TERM}" \
      with_timeout 30 "${root}/bin/nmail" </dev/null 2>&1 || true)"
    [[ "${out}" == *"${SENTINEL}"* ]] && cfg="cfg:ok" || cfg="cfg:WARN"
    rm -rf "${home}"
  fi
  if [[ "${TUI_PROBE}" != "0" ]]; then
    home="$(mktemp -d)"
    out="$(printf 'q\n' | HOME="${home}" TERM="${SMOKE_TERM}" \
      with_timeout 20 "${root}/bin/nmail" -k 2>&1 || true)"
    [[ "${out}" == *"${TUI_SENTINEL}"* ]] && tui="tui:ok" || tui="tui:WARN"
    rm -rf "${home}"
  fi
  echo "  PASS ${target} @ host  (${cfg} ${tui})"
  record "PASS ${target} @ host ${cfg} ${tui}"; PASS=$((PASS + 1))

  rm -rf "${tree}"
}

TARBALLS=()
while IFS= read -r line; do
  [[ -n "${line}" ]] && TARBALLS+=("${line}")
done < <(resolve_args "$@")
if [[ ${#TARBALLS[@]} -eq 0 ]]; then
  echo "$0: no tarballs to test (run package.sh first)" >&2
  exit 1
fi

for tarball in "${TARBALLS[@]}"; do
  base="$(basename "${tarball}")"
  target="$(target_of "${base}")"
  if [[ -z "${target}" ]]; then
    echo "skip ${base}: unrecognized target"
    record "SKIP ${base} (unknown target)"; SKIP=$((SKIP + 1)); continue
  fi

  echo "== ${base} =="
  case "${target}" in
    macos-*)
      if [[ "${HOST_OS}" != "Darwin" || "${HOST_ARCH}" != "arm64" ]]; then
        echo "  SKIP ${target}: run on a macOS arm64 host"
        record "SKIP ${target} (needs macOS arm64 host)"; SKIP=$((SKIP + 1))
        continue
      fi
      smoke_macos "${tarball}" "${target}"
      ;;
    linux-*)
      arch="arm64"; [[ "${target}" == *x86_64* ]] && arch="x86_64"
      if [[ "${arch}" != "${HOST_ARCH}" ]]; then
        echo "  SKIP ${target}: ${arch} artifact, host is ${HOST_ARCH} (run on a ${arch} host)"
        record "SKIP ${target} (arch ${arch} != host ${HOST_ARCH})"; SKIP=$((SKIP + 1))
        continue
      fi
      smoke_linux "${tarball}" "${target}" "${arch}"
      ;;
  esac
done

echo
echo "== smoke summary =="
for line in "${SUMMARY[@]}"; do echo "  ${line}"; done
echo "  ---"
echo "  pass ${PASS}  fail ${FAIL}  skip ${SKIP}"
[[ ${FAIL} -eq 0 ]]
