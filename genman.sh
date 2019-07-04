#!/bin/bash

# genman.sh builds application and (re-)generates its man-page

mkdir -p build && cd build && cmake .. && make -s && cd .. && \
help2man -n "ncurses mail" -N -o src/nmail.1 ./build/nmail
exit ${?}

