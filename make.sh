#!/usr/bin/env bash

# make.sh
#
# Copyright (C) 2020-2025 Kristofer Berggren
# All rights reserved.
#
# See LICENSE for redistribution information.

# exiterr
exiterr()
{
  >&2 echo "${1}"
  exit 1
}

# process arguments
DEPS="0"
BUILD="0"
DEBUG="0"
TESTS="0"
DOC="0"
INSTALL="0"
SRC="0"
BUMP="0"
case "${1%/}" in
  deps)
    DEPS="1"
    ;;

  build)
    BUILD="1"
    ;;

  debug)
    DEBUG="1"
    ;;

  test*)
    BUILD="1"
    TESTS="1"
    ;;

  doc)
    BUILD="1"
    DOC="1"
    ;;

  install)
    BUILD="1"
    INSTALL="1"
    ;;

  src)
    SRC="1"
    ;;

  bump)
    BUMP="1"
    ;;

  all)
    DEPS="1"
    BUILD="1"
    TESTS="1"
    DOC="1"
    INSTALL="1"
    ;;

  *)
    echo "usage: make.sh <deps|build|tests|doc|install|all>"
    echo "  deps      - install project dependencies"
    echo "  build     - perform build"
    echo "  debug     - perform debug build"
    echo "  tests     - perform build and run tests"
    echo "  doc       - perform build and generate documentation"
    echo "  install   - perform build and install"
    echo "  all       - perform deps, build, tests, doc and install"
    echo "  src       - perform source code reformatting"
    echo "  bump      - perform version bump"
    exit 1
    ;;
esac

# detect os / distro
OS="$(uname)"
if [ "${OS}" == "Linux" ]; then
  unset NAME
  eval $(grep "^NAME=" /etc/os-release 2> /dev/null)
  if [[ "${NAME}" != "" ]]; then
    DISTRO="${NAME}"
  else
    if [[ "${TERMUX_VERSION}" != "" ]]; then
      DISTRO="Termux"
    fi
  fi
fi

# set make / cmake args
if [[ "${BUILD}" == "1" ]] || [[ "${DEBUG}" == "1" ]]; then
  MAKEARGS=""
  CMAKEARGS=""
  if [ "${OS}" == "Linux" ]; then
    MAKEARGS="-j$(nproc) ${MAKEARGS}"
    if [[ "${DISTRO}" == "Termux" ]]; then
      CMAKEARGS="-DCMAKE_INSTALL_PREFIX=${PREFIX} ${CMAKEARGS}"
      export CC="clang"
      export CXX="clang++"
    fi
  elif [ "${OS}" == "Darwin" ]; then
    MAKEARGS="-j$(sysctl -n hw.ncpu) ${MAKEARGS}"
  fi
fi

# deps
if [[ "${DEPS}" == "1" ]]; then
  if [ "${OS}" == "Linux" ]; then
    if [[ "${DISTRO}" == "Ubuntu" ]] || [[ "${DISTRO}" == "Raspbian GNU/Linux" ]] || [[ "${DISTRO}" == "Debian GNU/Linux" ]] || [[ "${DISTRO}" == "Pop!_OS" ]] || [[ "${DISTRO}" == "Linux Mint" ]]; then
      sudo apt update && sudo apt -y install cmake build-essential libssl-dev libreadline-dev libncurses5-dev libxapian-dev libsqlite3-dev libsasl2-dev libsasl2-modules libcurl4-openssl-dev libexpat-dev zlib1g-dev libmagic-dev uuid-dev w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Fedora" ]] || [[ "${DISTRO}" == "Fedora Linux" ]] || [[ "${DISTRO}" == "Rocky Linux" ]]; then
      sudo yum -y install cmake openssl-devel ncurses-devel xapian-core-devel sqlite-devel cyrus-sasl-devel cyrus-sasl-plain libcurl-devel expat-devel zlib-devel file-devel libuuid-devel clang w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Arch Linux" ]] || [[ "${DISTRO}" == "Arch Linux ARM" ]]; then
      sudo pacman --needed -Sy cmake make openssl ncurses xapian-core sqlite cyrus-sasl curl expat zlib file w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Gentoo" ]]; then
      sudo emerge -n dev-build/cmake dev-libs/openssl sys-libs/ncurses dev-libs/xapian dev-db/sqlite dev-libs/cyrus-sasl net-misc/curl dev-libs/expat sys-libs/zlib sys-apps/file w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Alpine Linux" ]]; then
      sudo apk add git build-base cmake ncurses-dev openssl-dev xapian-core-dev sqlite-dev curl-dev expat-dev cyrus-sasl-dev cyrus-sasl-login file-dev util-linux-dev zlib-dev linux-headers w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "openSUSE Tumbleweed" ]]; then
      sudo zypper install -y -t pattern devel_C_C++ && sudo zypper install -y cmake libopenssl-devel libxapian-devel sqlite3-devel libcurl-devel libexpat-devel file-devel w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Void" ]]; then
      sudo xbps-install -y base-devel ccache cmake openssl-devel xapian-core-devel sqlite-devel libcurl-devel expat-devel libsasl-devel cyrus-sasl-modules file-devel w3m || exiterr "deps failed (${DISTRO}), exiting."
    elif [[ "${DISTRO}" == "Termux" ]]; then
      pkg install git cmake clang ccache libxapian libsqlite libsasl file libandroid-wordexp libandroid-glob libandroid-posix-semaphore w3m || exiterr "deps failed (${DISTRO}), exiting."
    else
      exiterr "deps failed (unsupported linux distro ${DISTRO}), exiting."
    fi
  elif [ "${OS}" == "Darwin" ]; then
    if command -v brew &> /dev/null; then
      HOMEBREW_NO_AUTO_UPDATE=1 brew install cmake openssl ncurses xapian sqlite libmagic ossp-uuid w3m || exiterr "deps failed (${OS} brew), exiting."
    elif command -v port &> /dev/null; then
      sudo port -N install openssl ncurses xapian-core sqlite3 libmagic ossp-uuid w3m || exiterr "deps failed (${OS} port), exiting."
    else
      exiterr "deps failed (${OS} missing brew and port), exiting."
    fi
  else
    exiterr "deps failed (unsupported os ${OS}), exiting."
  fi
fi

# src
if [[ "${SRC}" == "1" ]]; then
  uncrustify -c etc/uncrustify.cfg --replace --no-backup src/*.cpp src/*.h || \
    exiterr "unrustify failed, exiting."
fi

# bump
if [[ "${BUMP}" == "1" ]]; then
  CURRENT_VERSION=$(grep NMAIL_VERSION src/version.cpp | head -1 | awk -F'"' '{print $2}') # ex: 5.1.1
  CURRENT_MAJMIN="$(echo ${CURRENT_VERSION} | cut -d'.' -f1,2)" # ex: 5.1
  URL="https://github.com/d99kris/nmail.git"
  LATEST_TAG=$(git -c 'versionsort.suffix=-' ls-remote --tags --sort='v:refname' ${URL} | tail -n1 | cut -d'/' -f3)
  LATEST_VERSION=$(echo "${LATEST_TAG}" | cut -c2-) # ex: 5.1.3
  LATEST_MAJMIN="$(echo ${LATEST_VERSION} | cut -d'.' -f1,2)" # ex: 5.1
  SED="sed"
  if [[ "$(uname)" == "Darwin" ]]; then
    SED="gsed"
  fi
  if [[ "${CURRENT_MAJMIN}" == "${LATEST_MAJMIN}" ]]; then
    NEW_MAJ="$(echo ${CURRENT_VERSION} | cut -d'.' -f1)" # ex: 5
    let NEW_MIN=$(echo ${CURRENT_VERSION} | cut -d'.' -f2)+1
    NEW_PATCH="1" # use 1-based build/snapshot number
    NEW_VERSION="${NEW_MAJ}.${NEW_MIN}.${NEW_PATCH}"
    echo "Current:      ${CURRENT_MAJMIN} == ${LATEST_MAJMIN} Latest"
    echo "Bump release: ${NEW_VERSION}"
    ${SED} -i "s/^#define NMAIL_VERSION .*/#define NMAIL_VERSION \"${NEW_VERSION}\"/g" src/version.cpp
  else
    NEW_MAJ="$(echo ${CURRENT_VERSION} | cut -d'.' -f1)" # ex: 5
    NEW_MIN="$(echo ${CURRENT_VERSION} | cut -d'.' -f2)" # ex: 1
    let NEW_PATCH=$(echo ${CURRENT_VERSION} | cut -d'.' -f3)+1
    NEW_VERSION="${NEW_MAJ}.${NEW_MIN}.${NEW_PATCH}"
    echo "Current:      ${CURRENT_MAJMIN} != ${LATEST_MAJMIN} Latest"
    echo "Bump build:   ${NEW_VERSION}"
    ${SED} -i "s/^#define NMAIL_VERSION .*/#define NMAIL_VERSION \"${NEW_VERSION}\"/g" src/version.cpp
  fi
fi

# build
if [[ "${BUILD}" == "1" ]]; then
  echo "-- Using cmake ${CMAKEARGS}"
  echo "-- Using make ${MAKEARGS}"
  mkdir -p build && cd build && cmake ${CMAKEARGS} .. && make ${MAKEARGS} && cd .. || exiterr "build failed, exiting."
fi

# debug
if [[ "${DEBUG}" == "1" ]]; then
  CMAKEARGS="-DCMAKE_BUILD_TYPE=Debug ${CMAKEARGS}"
  echo "-- Using cmake ${CMAKEARGS}"
  echo "-- Using make ${MAKEARGS}"
  mkdir -p dbgbuild && cd dbgbuild && cmake ${CMAKEARGS} .. && make ${MAKEARGS} && cd .. || exiterr "debug build failed, exiting."
fi

# tests
if [[ "${TESTS}" == "1" ]]; then
  true # currently this project has no tests
fi

# doc
if [[ "${DOC}" == "1" ]]; then
  if [[ -x "$(command -v help2man)" ]]; then
    if [[ "$(uname)" == "Darwin" ]]; then
      SED="gsed -i"
    else
      SED="sed -i"
    fi
    help2man -n "ncurses mail" -N -o src/nmail.1 ./build/nmail && ${SED} "s/\.\\\\\" DO NOT MODIFY THIS FILE\!  It was generated by help2man.*/\.\\\\\" DO NOT MODIFY THIS FILE\!  It was generated by help2man./g" src/nmail.1 || exiterr "doc failed, exiting."
  fi
fi

# install
if [[ "${INSTALL}" == "1" ]]; then
  if [[ -z ${INSTALL_CMD+x} ]]; then
    if [[ "${OS}" == "Linux" ]]; then
      if [[ "${DISTRO}" != "Termux" ]]; then
        INSTALL_CMD="$(basename $(which sudo doas | head -1))"
      fi
    elif [[ "${OS}" == "Darwin" ]]; then
      if [[ "${GITHUB_ACTIONS}" == "true" ]]; then
        INSTALL_CMD="sudo"
      fi
    fi
  fi

  echo "-- Using ${INSTALL_CMD:+$INSTALL_CMD }make install"
  cd build && ${INSTALL_CMD} make install && cd .. || exiterr "install failed (${OS}), exiting."
fi

# exit
exit 0
