# embedthemes.cmake
#
# Copyright (c) 2026 Kristofer Berggren
# All rights reserved.
#
# nmail is distributed under the MIT license, see LICENSE for details.
#
# Generates a C++ header embedding the themes listed in index.txt, as unsigned
# char arrays and an EmbeddedThemes table in index order.
# Usage:
#   cmake -DTHEMES_DIR=<path> -DOUT_FILE=<path> -P embedthemes.cmake

if(NOT THEMES_DIR OR NOT OUT_FILE)
  message(FATAL_ERROR "usage: cmake -DTHEMES_DIR=<path> -DOUT_FILE=<path> -P embedthemes.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/embedutil.cmake")
embed_theme_names("${THEMES_DIR}/index.txt" THEME_NAMES)
if(NOT THEME_NAMES)
  message(FATAL_ERROR "no themes listed in ${THEMES_DIR}/index.txt")
endif()

get_filename_component(OUT_NAME "${OUT_FILE}" NAME)

set(CONTENT "")
string(APPEND CONTENT
  "// ${OUT_NAME} - generated from index.txt by embedthemes.cmake, do not edit\n"
  "\n"
  "#pragma once\n"
  "\n"
  "#include <cstddef>\n"
  "\n"
  "struct EmbeddedTheme\n"
  "{\n"
  "  const char* name;\n"
  "  const unsigned char* data;\n"
  "  size_t size;\n"
  "};\n")

set(TABLE "")
foreach(THEME_NAME ${THEME_NAMES})
  set(THEME_FILE "${THEMES_DIR}/${THEME_NAME}.conf")
  if(NOT EXISTS "${THEME_FILE}")
    message(FATAL_ERROR "theme ${THEME_NAME} listed in index.txt not found: ${THEME_FILE}")
  endif()

  embed_hex_bytes("${THEME_FILE}" BYTES)
  string(MAKE_C_IDENTIFIER "Theme_${THEME_NAME}" VAR_NAME)
  string(APPEND CONTENT
    "\n"
    "static const unsigned char ${VAR_NAME}[] =\n"
    "{\n"
    "  ${BYTES}\n"
    "};\n")
  string(APPEND TABLE "  { \"${THEME_NAME}\", ${VAR_NAME}, sizeof(${VAR_NAME}) },\n")
endforeach()

string(APPEND CONTENT
  "\n"
  "static const EmbeddedTheme EmbeddedThemes[] =\n"
  "{\n"
  "${TABLE}"
  "};\n")

file(WRITE "${OUT_FILE}" "${CONTENT}")
