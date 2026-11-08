#!/usr/bin/env bash

# make.sh
#
# Copyright (c) 2026 Kristofer Berggren
# All rights reserved.
#
# nhtmlmd is distributed under the MIT license, see LICENSE for details.

# exiterr
exiterr()
{
  >&2 echo "${1}"
  exit 1
}

# process arguments
SRC="0"
BUILD="0"
TESTS="0"
case "${1%/}" in
  src)
    SRC="1"
    ;;

  build)
    BUILD="1"
    ;;

  test*)
    BUILD="1"
    TESTS="1"
    ;;

  all)
    SRC="1"
    BUILD="1"
    TESTS="1"
    ;;

  *)
    echo "usage: make.sh <src|build|tests|all>"
    echo "  src       - reformat source code"
    echo "  build     - perform build"
    echo "  tests     - perform build and run tests"
    echo "  all       - perform all actions above"
    exit 1
    ;;
esac

cd "$(dirname "${0}")" || exiterr "cd failed, exiting."

# src
if [[ "${SRC}" == "1" ]]; then
  uncrustify -c etc/uncrustify.cfg --replace --no-backup src/*.cpp src/*.h || \
    exiterr "uncrustify failed, exiting."
fi

# build
if [[ "${BUILD}" == "1" ]]; then
  OS="$(uname)"
  MAKEARGS=""
  if [ "${OS}" == "Linux" ]; then
    MAKEARGS="-j$(nproc)"
  elif [ "${OS}" == "Darwin" ]; then
    MAKEARGS="-j$(sysctl -n hw.ncpu)"
  fi
  mkdir -p build-debug && cd build-debug && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -s ${MAKEARGS} && cd .. && \
  mkdir -p build-release && cd build-release && cmake -DCMAKE_BUILD_TYPE=Release .. && make -s ${MAKEARGS} && cd .. || \
  exiterr "build failed, exiting."
fi

# tests
if [[ "${TESTS}" == "1" ]]; then
  cd build-debug && ctest --output-on-failure && cd .. && \
  cd build-release && ctest --output-on-failure && cd .. || \
  exiterr "tests failed, exiting."
fi

# exit
exit 0
