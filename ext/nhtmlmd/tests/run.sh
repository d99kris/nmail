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

for HTML in html2md/*.html; do
  BASE="${HTML%.html}"
  "${NHTMLMD}" "${HTML}" > "${ACTUAL}" && check "${BASE}.md"
  if [[ -f "${BASE}.txt" || "${UPDATE}" == "1" ]]; then
    "${NHTMLMD}" -t "${HTML}" > "${ACTUAL}" && check "${BASE}.txt"
  fi
  if [[ -f "${BASE}.refs.md" ]]; then
    "${NHTMLMD}" -l refs "${HTML}" > "${ACTUAL}" && check "${BASE}.refs.md"
  fi
  if [[ -f "${BASE}.inline.md" ]]; then
    "${NHTMLMD}" -l inline "${HTML}" > "${ACTUAL}" && check "${BASE}.inline.md"
  fi
done

for MD in roundtrip/*.md; do
  [[ "${MD}" == *.rt.md ]] && continue
  "${NHTMLMD}" -m -b "${MD}" | "${NHTMLMD}" -l inline > "${ACTUAL}" && check "${MD%.md}.rt.md"
done

for MD in md2html/*.md; do
  "${NHTMLMD}" -m -b "${MD}" > "${ACTUAL}" && check "${MD%.md}.html"
done

if [[ "${UPDATE}" == "0" ]]; then
  echo "${COUNT} tests, ${FAILED} failed"
fi

exit $((FAILED > 0))
