# Project
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(nhtmlmd VERSION 0.3.0 LANGUAGES C CXX)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(NOT CMAKE_BUILD_TYPE AND (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR))
  set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(NHTMLMD_USE_SYSTEM_LEXBOR "Use system lexbor" OFF)
option(NHTMLMD_USE_SYSTEM_MD4C "Use system md4c" OFF)
message(STATUS "nhtmlmd system lexbor: ${NHTMLMD_USE_SYSTEM_LEXBOR}")
message(STATUS "nhtmlmd system md4c: ${NHTMLMD_USE_SYSTEM_MD4C}")

# Library
add_library(nhtmlmd STATIC
  src/nhtmlmd.cpp
  src/nhtmlmd.h
  src/node.h
  src/util.cpp
  src/util.h
  src/walker.cpp
  src/walker.h
  src/writer.cpp
  src/writer.h
)
target_include_directories(nhtmlmd PUBLIC src)
set_target_properties(nhtmlmd PROPERTIES COMPILE_FLAGS
                      "-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith \
                       -Wcast-qual -Wno-missing-braces -Wswitch-default \
                       -Wunreachable-code -Wuninitialized -Wcast-align")

# Dependency lexbor (core dom html ns tag)
if(NHTMLMD_USE_SYSTEM_LEXBOR)
  find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/html/html.h)
  find_library(LEXBOR_LIBRARY NAMES lexbor_static lexbor)
  if(NOT LEXBOR_INCLUDE_DIR OR NOT LEXBOR_LIBRARY)
    message(FATAL_ERROR "lexbor not found")
  endif()
  target_include_directories(nhtmlmd SYSTEM PRIVATE ${LEXBOR_INCLUDE_DIR})
  target_link_libraries(nhtmlmd PRIVATE ${LEXBOR_LIBRARY})
else()
  file(GLOB_RECURSE LEXBOR_SOURCES CONFIGURE_DEPENDS "ext/lexbor/source/lexbor/*.c")
  add_library(nhtmlmd_lexbor STATIC ${LEXBOR_SOURCES})
  target_include_directories(nhtmlmd_lexbor SYSTEM PUBLIC ext/lexbor/source)
  target_compile_definitions(nhtmlmd_lexbor PUBLIC LEXBOR_STATIC)
  target_link_libraries(nhtmlmd PRIVATE nhtmlmd_lexbor)
endif()

# Dependency md4c
if(NHTMLMD_USE_SYSTEM_MD4C)
  find_path(MD4C_INCLUDE_DIR NAMES md4c-html.h)
  find_library(MD4C_HTML_LIBRARY NAMES md4c-html)
  find_library(MD4C_LIBRARY NAMES md4c)
  if(NOT MD4C_INCLUDE_DIR OR NOT MD4C_HTML_LIBRARY OR NOT MD4C_LIBRARY)
    message(FATAL_ERROR "md4c not found")
  endif()
  target_include_directories(nhtmlmd SYSTEM PRIVATE ${MD4C_INCLUDE_DIR})
  target_link_libraries(nhtmlmd PRIVATE ${MD4C_HTML_LIBRARY} ${MD4C_LIBRARY})
else()
  add_library(nhtmlmd_md4c STATIC
    ext/md4c/src/entity.c
    ext/md4c/src/md4c-html.c
    ext/md4c/src/md4c.c
  )
  target_include_directories(nhtmlmd_md4c SYSTEM PUBLIC ext/md4c/src)
  target_link_libraries(nhtmlmd PRIVATE nhtmlmd_md4c)
endif()

# Utility (built, not installed)
add_executable(nhtmlmd-cli src/main.cpp)
set_target_properties(nhtmlmd-cli PROPERTIES OUTPUT_NAME nhtmlmd)
target_link_libraries(nhtmlmd-cli PRIVATE nhtmlmd)

# Tests (standalone build only)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  enable_testing()
  add_test(NAME nhtmlmd COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:nhtmlmd-cli>)
endif()
