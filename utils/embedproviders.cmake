# embedproviders.cmake
#
# Copyright (c) 2026 Kristofer Berggren
# All rights reserved.
#
# nmail is distributed under the MIT license, see LICENSE for details.
#
# Generates a C++ header embedding all providers/*.conf files, as unsigned char
# arrays and an EmbeddedProviders table sorted by provider id.
# Usage:
#   cmake -DPROVIDERS_DIR=<path> -DOUT_FILE=<path> -P embedproviders.cmake

if(NOT PROVIDERS_DIR OR NOT OUT_FILE)
  message(FATAL_ERROR "usage: cmake -DPROVIDERS_DIR=<path> -DOUT_FILE=<path> -P embedproviders.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/embedutil.cmake")
file(GLOB PROVIDER_FILES "${PROVIDERS_DIR}/*.conf")
list(SORT PROVIDER_FILES)
if(NOT PROVIDER_FILES)
  message(FATAL_ERROR "no providers found in ${PROVIDERS_DIR}")
endif()

get_filename_component(OUT_NAME "${OUT_FILE}" NAME)

set(CONTENT "")
string(APPEND CONTENT
  "// ${OUT_NAME} - generated from providers/*.conf by embedproviders.cmake, do not edit\n"
  "\n"
  "#pragma once\n"
  "\n"
  "#include <cstddef>\n"
  "\n"
  "struct EmbeddedProvider\n"
  "{\n"
  "  const char* id;\n"
  "  const unsigned char* data;\n"
  "  size_t size;\n"
  "};\n")

set(TABLE "")
foreach(PROVIDER_FILE ${PROVIDER_FILES})
  get_filename_component(PROVIDER_ID "${PROVIDER_FILE}" NAME_WE)
  embed_hex_bytes("${PROVIDER_FILE}" BYTES)
  string(MAKE_C_IDENTIFIER "Provider_${PROVIDER_ID}" VAR_NAME)
  string(APPEND CONTENT
    "\n"
    "static const unsigned char ${VAR_NAME}[] =\n"
    "{\n"
    "  ${BYTES}\n"
    "};\n")
  string(APPEND TABLE "  { \"${PROVIDER_ID}\", ${VAR_NAME}, sizeof(${VAR_NAME}) },\n")
endforeach()

string(APPEND CONTENT
  "\n"
  "static const EmbeddedProvider EmbeddedProviders[] =\n"
  "{\n"
  "${TABLE}"
  "};\n")

file(WRITE "${OUT_FILE}" "${CONTENT}")
