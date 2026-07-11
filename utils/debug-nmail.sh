#!/usr/bin/env bash

PROG="$(which nmail)"
ARGS="$@"

# Detect debugger
DBG="$(which gdb | head -1)"
if [[ "${DBG}" == "" ]]; then
  echo "missing debugger - please install gdb, exiting."
  exit 1
fi

if [[ "$(basename ${DBG})" == "gdb" ]]; then
  echo "gdb"
  TMPDIR="$(mktemp -d)"
  GDBSCRIPT="${TMPDIR}/gdb.txt"
  cat <<EOF > ${GDBSCRIPT}
set print thread-events off
set print inferior-events off
set pagination off
set height unlimited
set logging file ${TMPDIR}/log.txt
set logging on
run ${ARGS}
echo \n\n
echo thread apply all bt\n
thread apply all bt
set logging off
quit
EOF
  echo "gdb -q -x \"${GDBSCRIPT}\" \"${PROG}\""
  OUTFILE="/tmp/nmail-gdb-${$}.txt"
  echo "stderr:" > ${OUTFILE}
  gdb -q -x "${GDBSCRIPT}" "${PROG}" 2>> ${OUTFILE}
  reset
  clear
  echo "gdb:" >> ${OUTFILE}
  cat ${TMPDIR}/log.txt >> ${OUTFILE}
  rm -rf ${TMPDIR}
  echo ""
  echo "gdb log file written to:"
  echo "${OUTFILE}"
else
  echo "unknown debugger ${DBG}, exiting."
  exit 1
fi

