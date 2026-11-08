#!/usr/bin/env bash

# run.sh
#
# Copyright (c) 2026 Kristofer Berggren
# All rights reserved.
#
# nhtmlmd is distributed under the MIT license, see LICENSE for details.
#
# Golden output tests. Usage: run.sh [--update] <nhtmlmd-binary>
#   html2md/X.html   -> X.md (default), X.txt (-t), X.refs.md (-l refs),
#                       X.inline.md (-l inline), each when present
#   roundtrip/X.md   -> X.rt.md (-m -b, then -l inline)
#   md2html/X.md     -> X.html (-m -b)
# Stress tests (generated hostile html) must complete with limited stack and time.

UPDATE=0
if [[ "${1}" == "--update" ]]; then
  UPDATE=1
  shift
fi

NHTMLMD="${1}"
if [[ ! -x "${NHTMLMD}" ]]; then
  >&2 echo "usage: run.sh [--update] <nhtmlmd-binary>"
  exit 1
fi

NHTMLMD="$(cd "$(dirname "${NHTMLMD}")" && pwd)/$(basename "${NHTMLMD}")"
cd "$(dirname "${0}")" || exit 1

FAILED=0
COUNT=0
ACTUAL="$(mktemp)"
trap 'rm -f "${ACTUAL}"' EXIT

# compares output in ${ACTUAL} byte by byte with expected file ${1}
check()
{
  local EXPECTED="${1}"
  COUNT=$((COUNT + 1))
  if [[ "${UPDATE}" == "1" ]]; then
    cp "${ACTUAL}" "${EXPECTED}"
    echo "updated ${EXPECTED}"
  elif cmp -s "${EXPECTED}" "${ACTUAL}"; then
    echo "ok      ${EXPECTED}"
  else
    echo "FAILED  ${EXPECTED}"
    diff -u "${EXPECTED}" "${ACTUAL}" | head -20
    FAILED=$((FAILED + 1))
  fi
}

# output of a failed run is compared too, so it fails the test
for HTML in html2md/*.html; do
  BASE="${HTML%.html}"
  "${NHTMLMD}" "${HTML}" > "${ACTUAL}"; check "${BASE}.md"
  if [[ -f "${BASE}.txt" || "${UPDATE}" == "1" ]]; then
    "${NHTMLMD}" -t "${HTML}" > "${ACTUAL}"; check "${BASE}.txt"
  fi
  if [[ -f "${BASE}.refs.md" ]]; then
    "${NHTMLMD}" -l refs "${HTML}" > "${ACTUAL}"; check "${BASE}.refs.md"
  fi
  if [[ -f "${BASE}.inline.md" ]]; then
    "${NHTMLMD}" -l inline "${HTML}" > "${ACTUAL}"; check "${BASE}.inline.md"
  fi
done

for MD in roundtrip/*.md; do
  [[ "${MD}" == *.rt.md ]] && continue
  "${NHTMLMD}" -m -b "${MD}" | "${NHTMLMD}" -l inline > "${ACTUAL}"; check "${MD%.md}.rt.md"
done

for MD in md2html/*.md; do
  "${NHTMLMD}" -m -b "${MD}" > "${ACTUAL}"; check "${MD%.md}.html"
done

# repeats string ${1} ${2} times
repeat()
{
  local STR=""
  for ((I = 0; I < ${2}; ++I)); do
    STR+="${1}"
  done
  printf '%s' "${STR}"
}

# runs nhtmlmd with args ${@} on ${INPUT}, with stack and time limited as on a worker thread,
# to catch deep recursion and exponential time on hostile html
stress()
{
  local NAME="${1}"
  shift
  COUNT=$((COUNT + 1))
  ( ulimit -s 512 && exec "${NHTMLMD}" "${@}" "${INPUT}" > /dev/null 2>&1 ) &
  local PID=${!}
  ( sleep 10 && kill -9 "${PID}" ) > /dev/null 2>&1 &
  local WATCHER=${!}
  wait "${PID}" 2> /dev/null
  local RV=${?}
  kill "${WATCHER}" > /dev/null 2>&1
  wait "${WATCHER}" 2> /dev/null
  if [[ "${RV}" == "0" ]]; then
    echo "ok      stress ${NAME}${*:+ ${*}}"
  else
    echo "FAILED  stress ${NAME}${*:+ ${*}} (exit ${RV})"
    FAILED=$((FAILED + 1))
  fi
}

if [[ "${UPDATE}" == "0" ]]; then
  INPUT="$(mktemp)"
  trap 'rm -f "${ACTUAL}" "${INPUT}"' EXIT
  STRESS=(
    "div:$(repeat '<div>' 5000)x<br>y"
    "bold:$(repeat '<b>' 5000)x<br>y"
    "table:$(repeat '<table><tr><td>' 2000)x<br>y"
    "quote-list:$(repeat '<blockquote><ul><li>' 2000)x<br>y"
    "bold-lines:$(repeat '<b>' 40)a<br>b"
    "styles-lines:$(repeat '<b><i><s><sup><del><code><em><strong>' 8)a<br>b"
    "sup-long:$(repeat '<sup>' 40)hello world"
    "link-list:$(repeat '<a href="https://x/"><b><ol><li>' 40)a<br>b"
  )
  for ENTRY in "${STRESS[@]}"; do
    printf '%s' "${ENTRY#*:}" > "${INPUT}"
    stress "${ENTRY%%:*}"
    stress "${ENTRY%%:*}" -t
    stress "${ENTRY%%:*}" -l refs
  done
fi

if [[ "${UPDATE}" == "0" ]]; then
  echo "${COUNT} tests, ${FAILED} failed"
fi

exit $((FAILED > 0))
