# Project
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(nmail VERSION "2.86" LANGUAGES CXX C)
include(CheckIncludeFile)
set(CMAKE_CXX_STANDARD 11)

# Ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Build type
set(DEFAULT_BUILD_TYPE "Release")
if((EXISTS "${CMAKE_SOURCE_DIR}/.svn") OR (EXISTS "${CMAKE_SOURCE_DIR}/.git"))
  set(DEFAULT_BUILD_TYPE "Debug")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Using build type '${DEFAULT_BUILD_TYPE}' (default).")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
else()
  message(STATUS "Using build type '${CMAKE_BUILD_TYPE}'.")
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Application
add_executable(nmail
  ext/apathy/path.hpp
  ext/cxx-prettyprint/prettyprint.hpp
  ext/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h
  src/addressbook.cpp
  src/addressbook.h
  src/auth.cpp
  src/auth.h
  src/body.cpp
  src/body.h
  src/cacheutil.cpp
  src/cacheutil.h
  src/config.cpp
  src/config.h
  src/contact.cpp
  src/contact.h
  src/crypto.cpp
  src/crypto.h
  src/flag.cpp
  src/flag.h
  src/header.cpp
  src/header.h
  src/imap.cpp
  src/imap.h
  src/imapcache.cpp
  src/imapcache.h
  src/imapindex.cpp
  src/imapindex.h
  src/imapmanager.cpp
  src/imapmanager.h
  src/lockfile.cpp
  src/lockfile.h
  src/log.cpp
  src/log.h
  src/loghelp.cpp
  src/loghelp.h
  src/main.cpp
  src/offlinequeue.cpp
  src/offlinequeue.h
  src/sasl.cpp
  src/sasl.h
  src/searchengine.cpp
  src/searchengine.h
  src/serialized.cpp
  src/serialized.h
  src/smtp.cpp
  src/smtp.h
  src/smtpmanager.cpp
  src/smtpmanager.h
  src/status.cpp
  src/status.h
  src/ui.cpp
  src/ui.h
  src/util.cpp
  src/util.h
)
install(TARGETS nmail DESTINATION bin)

# Platform specifics
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_compile_definitions(_XOPEN_SOURCE_EXTENDED)
  set(CMAKE_PREFIX_PATH /usr/local/opt/ncurses)
  set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
  FIND_LIBRARY(CARBON_LIBRARY CoreFoundation)
  FIND_LIBRARY(CARBON_LIBRARY CFNetwork)
  FIND_LIBRARY(CARBON_LIBRARY Security)
  target_link_libraries(nmail PUBLIC iconv z "-framework CoreFoundation" "-framework Security" "-framework CFNetwork")
endif()

# Headers
target_include_directories(nmail PRIVATE "ext")

# Compiler flags
set_target_properties(nmail PROPERTIES COMPILE_FLAGS
                      "-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith \
                       -Wcast-qual -Wno-missing-braces -Wswitch-default \
                       -Wunreachable-code -Wuninitialized -Wcast-align")
# todo: add -Wundef

target_compile_definitions(nmail PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# Dependency ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(Xapian REQUIRED)

find_package(SQLite3 REQUIRED)

find_path(LIBETPAN_INCLUDE_DIR
  NAMES libetpan/libetpan.h
  PATHS ${additional_includes}
)

find_library(LIBETPAN_LIBRARY
  NAMES etpan
  PATHS ${additional_lib_searchpath}
)

if(NOT LIBETPAN_INCLUDE_DIR OR NOT LIBETPAN_LIBRARY)
  message(FATAL_ERROR "ERROR: Could not find libetpan")
else()
  message(STATUS "Found libetpan: ${LIBETPAN_LIBRARY}")
endif()

find_library(CYRUS_SASL_LIBRARY sasl2)
find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args(sasl2 DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)

include_directories(${LIBETPAN_INCLUDE_DIR} ${CYRUS_SASL_INCLUDE_DIR} "ext/sqlite_modern_cpp/hdr")

CHECK_INCLUDE_FILE(execinfo.h FOUND_EXECINFO)
if(FOUND_EXECINFO)
  target_compile_definitions(nmail PRIVATE HAVE_EXECINFO_H=1)
endif()

# Linking
target_link_libraries(nmail PUBLIC
                      ${CURSES_NCURSES_LIBRARY} OpenSSL::SSL SQLite::SQLite3
                      ${XAPIAN_LIBRARIES} ${LIBETPAN_LIBRARY} ${CYRUS_SASL_LIBRARY}
                      pthread ${CMAKE_DL_LIBS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(nmail PUBLIC -rdynamic)
endif()

# Manual
install(FILES src/nmail.1 DESTINATION share/man/man1)

# Utils
configure_file(src/oauth2nmail.py ${CMAKE_CURRENT_BINARY_DIR}/oauth2nmail.py COPYONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/oauth2nmail.py DESTINATION bin)

# Themes
macro(add_theme themename)
  configure_file(themes/${themename} ${CMAKE_CURRENT_BINARY_DIR}/share/nmail/themes/${themename} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/share/nmail/themes/${themename} DESTINATION share/nmail/themes)
endmacro()
add_theme("default.conf")
add_theme("htop-style.conf")

# Uninstall
add_custom_target(uninstall
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/share/man/man1/nmail.1"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/oauth2nmail.py"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/share/nmail"
)
