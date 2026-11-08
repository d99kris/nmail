#!/usr/bin/env bash

# update-ext.sh
#
# Copyright (c) 2026 Kristofer Berggren
# All rights reserved.
#
# nhtmlmd is distributed under the MIT license, see LICENSE for details.
#
# Re-vendors the bundled subsets of lexbor and md4c at pinned versions.

LEXBOR_TAG="v3.0.0"
MD4C_TAG="v0.6.0"

exiterr()
{
  >&2 echo "${1}"
  exit 1
}

cd "$(dirname "${0}")/.." || exiterr "cd failed, exiting."
TMPDIR="$(mktemp -d)" || exiterr "mktemp failed, exiting."
trap 'rm -rf "${TMPDIR}"' EXIT

git clone -q --depth 1 --branch "${LEXBOR_TAG}" https://github.com/lexbor/lexbor.git "${TMPDIR}/lexbor" || \
  exiterr "lexbor clone failed, exiting."
git clone -q --depth 1 --branch "${MD4C_TAG}" https://github.com/mity/md4c.git "${TMPDIR}/md4c" || \
  exiterr "md4c clone failed, exiting."

rm -rf ext/lexbor ext/md4c
mkdir -p ext/lexbor/source/lexbor/ports/posix/lexbor ext/md4c/src

for MODULE in core dom html ns tag; do
  cp -R "${TMPDIR}/lexbor/source/lexbor/${MODULE}" ext/lexbor/source/lexbor/
done
cp -R "${TMPDIR}/lexbor/source/lexbor/ports/posix/lexbor/core" ext/lexbor/source/lexbor/ports/posix/lexbor/
cp "${TMPDIR}/lexbor/LICENSE" "${TMPDIR}/lexbor/NOTICE" ext/lexbor/
find ext/lexbor -name "config.cmake" -delete

for FILE in entity.c entity.h md4c-html.c md4c-html.h md4c.c md4c.h; do
  cp "${TMPDIR}/md4c/src/${FILE}" ext/md4c/src/
done
cp "${TMPDIR}/md4c/LICENSE.md" ext/md4c/

echo "updated lexbor ${LEXBOR_TAG} and md4c ${MD4C_TAG}"
