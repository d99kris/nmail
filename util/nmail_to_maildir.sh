#!/usr/bin/env bash

# nmail_to_maildir.sh
#
# Copyright (C) 2021 Kristofer Berggren
# All rights reserved.
#
# See LICENSE for redistribution information.

# Example ~/.muttrc to access exported Maildir in mutt:"
# set mbox_type=Maildir
# set spoolfile="~/Maildir"
# set folder="~/Maildir"
# set mask=".*"

# Warning: This script is a quick proof of concept hack. It produces a
# simple Maildir directory structure which may not be following standards.

NMAILCONFDIR="${1}"
MAILDIR="${2}"

error()
{
  >&2 echo "Error on line ${1}, aborting." 
  exit 1
}

if [[ ! -d "${NMAILCONFDIR}" ]] || [[ "${MAILDIR}" == "" ]] || [[ -d "${MAILDIR}" ]]; then
  echo "usage: ./util/nmail_to_maildir.sh <nmailconfdir> <outputmaildir>"
  echo "   ex: ./util/nmail_to_maildir.sh ~/.nmail ~/Maildir"
  echo ""
  echo " note: <nmailconfdir> must point to a nmail directory with cache_encrypt=0"
  echo "       <outputmaildir> must not previously exist"
  exit 1
fi

# Create top-level base structure
mkdir -p "${MAILDIR}/cur" || error ${LINENO}
mkdir -p "${MAILDIR}/new" || error ${LINENO}
mkdir -p "${MAILDIR}/tmp" || error ${LINENO}

# Iterate over cached dirs
echo "Copying..."
cd "${NMAILCONFDIR}"/cache/imap || error ${LINENO}
for DIRNAME in *; do
  if [[ ! -d "${DIRNAME}" ]]; then
    continue
  fi

  OUTPUTDIRNAME="$(echo "${DIRNAME}" | xxd -r -p | tr / _)"

  SRCPATH="${NMAILCONFDIR}/cache/imap/${DIRNAME}"
  DSTPATH="${MAILDIR}/.${OUTPUTDIRNAME}"

  echo "${SRCPATH}"
  echo " -> ${DSTPATH}"

  mkdir -p "${DSTPATH}/cur" || error ${LINENO}
  mkdir -p "${DSTPATH}/new" || error ${LINENO}
  mkdir -p "${DSTPATH}/tmp" || error ${LINENO}
  ls -1 "${SRCPATH}"/*.eml > /dev/null 2> /dev/null
  if [[ "${?}" == "0" ]]; then
    cp -a "${SRCPATH}"/*.eml "${DSTPATH}/cur/" || error ${LINENO}
  fi
done

echo "Done"
exit 0
