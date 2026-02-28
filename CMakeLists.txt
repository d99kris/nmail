# Project
cmake_minimum_required(VERSION 3.14...3.22 FATAL_ERROR)
project(nmail LANGUAGES CXX C)
include(CheckIncludeFile)
set(CMAKE_CXX_STANDARD 14)
message(STATUS "Using cmake ${CMAKE_VERSION}")

# Modules
include(CheckIncludeFiles)

# Ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Build type
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Using build type '${DEFAULT_BUILD_TYPE}' (default).")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
else()
  message(STATUS "Using build type '${CMAKE_BUILD_TYPE}'.")
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Feature - Core dump
option(HAS_COREDUMP "Core Dump" ON)
message(STATUS "Core Dump: ${HAS_COREDUMP}")

# Application
add_executable(nmail
  ext/apathy/path.hpp
  ext/cxx-prettyprint/prettyprint.hpp
  ext/cyrus-imap/lib/imapurl.c
  ext/cyrus-imap/lib/imapurl.h
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
  src/debuginfo.cpp
  src/debuginfo.h
  src/encoding.cpp
  src/encoding.h
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
  src/serialization.h
  src/sleepdetect.cpp
  src/sleepdetect.h
  src/smtp.cpp
  src/smtp.h
  src/smtpmanager.cpp
  src/smtpmanager.h
  src/sqlitehelp.cpp
  src/sqlitehelp.h
  src/status.cpp
  src/status.h
  src/ui.cpp
  src/ui.h
  src/uikeyconfig.cpp
  src/uikeyconfig.h
  src/uikeyinput.cpp
  src/uikeyinput.h
  src/util.cpp
  src/util.h
  src/version.cpp
  src/version.h
)
install(TARGETS nmail DESTINATION bin)

# Platform specifics
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_definitions(-D_XOPEN_SOURCE_EXTENDED)

# ncurses
  execute_process(COMMAND sh "-c"
                  "command -v brew &> /dev/null && brew --prefix ncurses 2> /dev/null | tr -d '\n'"
                  OUTPUT_VARIABLE NCURSES_ROOT_DIR)
  if (EXISTS "${NCURSES_ROOT_DIR}")
    message(STATUS "Ncurses cmake prefix '${NCURSES_ROOT_DIR}' (homebrew).")
    list(APPEND CMAKE_PREFIX_PATH ${NCURSES_ROOT_DIR})
  elseif (EXISTS "/opt/local/bin/port")
    set(NCURSES_ROOT_DIR /opt/local)
    message(STATUS "Ncurses cmake prefix '${NCURSES_ROOT_DIR}' (macports).")
    list(APPEND CMAKE_PREFIX_PATH ${NCURSES_ROOT_DIR})
  endif()

  # sqlite
  execute_process(COMMAND sh "-c"
                  "command -v brew &> /dev/null && brew --prefix sqlite 2> /dev/null | tr -d '\n'"
                  OUTPUT_VARIABLE SQLITE_ROOT_DIR)
  if (EXISTS "${SQLITE_ROOT_DIR}")
    message(STATUS "SQLite cmake prefix '${SQLITE_ROOT_DIR}' (homebrew).")
    list(APPEND CMAKE_PREFIX_PATH ${SQLITE_ROOT_DIR})
  elseif (EXISTS "/opt/local/bin/port")
    set(SQLITE_ROOT_DIR /opt/local)
    message(STATUS "SQLite cmake prefix '${SQLITE_ROOT_DIR}' (macports).")
    list(APPEND CMAKE_PREFIX_PATH ${SQLITE_ROOT_DIR})
  endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
  add_compile_definitions(_XOPEN_SOURCE_EXTENDED)
  if (DEFINED ENV{PREFIX} AND "$ENV{PREFIX}" MATCHES "^/data/data/com\\.termux/files/usr$")
    message(STATUS "Found termux")
    add_compile_definitions(HAVE_TERMUX)
  endif()
endif()

# Headers
target_include_directories(nmail PRIVATE "ext")

# Compiler flags
set_target_properties(nmail PROPERTIES COMPILE_FLAGS
                      "-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith \
                       -Wcast-qual -Wno-missing-braces -Wswitch-default \
                       -Wunreachable-code -Wuninitialized -Wcast-align")
# todo: add -Wundef

# Features
if(HAS_COREDUMP)
  target_compile_definitions(nmail PRIVATE HAS_COREDUMP="${HAS_COREDUMP}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # Core dump entitlements
    set(SIGNSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/util/sign")
    set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nmail.entitlements")
    install(CODE "execute_process(COMMAND
      \"${SIGNSCRIPT}\" \"${ENTITLEMENTS}\" \"${CMAKE_INSTALL_PREFIX}/bin/nmail\"
      )" COMPONENT Runtime)
  endif()
endif()

# Dependency ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
target_compile_options(nmail PUBLIC ${NCURSES_CFLAGS})

# Dependency openssl
find_package(OpenSSL REQUIRED)

# Dependency xapian
find_package(Xapian REQUIRED)

# Dependency sqlite3
find_package(SQLite3 REQUIRED)

# Dependency libetpan
option(HAS_CUSTOM_LIBETPAN "Custom libetpan" ON)
message(STATUS "Custom libetpan: ${HAS_CUSTOM_LIBETPAN}")
if(HAS_CUSTOM_LIBETPAN)
  set(LIBETPAN_STATIC ON)
  set(LIBETPAN_NOINSTALL ON)
  set(LIBETPAN_LIBNAME "etpan-nmail")
  add_subdirectory(ext/libetpan)
  add_dependencies(nmail etpan-nmail)
  target_compile_definitions(nmail PRIVATE LIBETPAN_CUSTOM=1)
  set(LIBETPAN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/ext/libetpan/include)
  set(LIBETPAN_LIBRARY etpan-nmail)
else()
  find_path(LIBETPAN_INCLUDE_DIR
    NAMES libetpan/libetpan.h
    PATHS ${additional_includes}
  )
  find_library(LIBETPAN_LIBRARY
    NAMES etpan
    PATHS ${additional_lib_searchpath}
  )
endif()

if(NOT LIBETPAN_INCLUDE_DIR OR NOT LIBETPAN_LIBRARY)
  message(FATAL_ERROR "ERROR: Could not find libetpan")
else()
  message(STATUS "Found libetpan: ${LIBETPAN_LIBRARY}")
endif()

# Dependency sasl2
find_library(CYRUS_SASL_LIBRARY sasl2)
find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args(sasl2 DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)

# Dependency execinfo
CHECK_INCLUDE_FILE(execinfo.h FOUND_EXECINFO)
if(FOUND_EXECINFO)
  target_compile_definitions(nmail PRIVATE HAVE_EXECINFO_H=1)
endif()

# Dependency magic
find_library(MAGIC_LIBRARY magic)
find_path(MAGIC_HEADERS magic.h)
if(NOT MAGIC_LIBRARY OR NOT MAGIC_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libmagic")
else()
  message(STATUS "Found libmagic: ${MAGIC_LIBRARY} and ${MAGIC_HEADERS}")
endif()

# Dependency libuuid
find_library(LIBUUID_LIBRARIES NAMES uuid)
find_path(LIBUUID_HEADERS uuid.h PATH_SUFFIXES uuid/)
if(NOT LIBUUID_LIBRARIES OR NOT LIBUUID_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libuuid")
else()
  message(STATUS "Found libuuid: ${LIBUUID_LIBRARIES} and ${LIBUUID_HEADERS}")
endif()

# Dependency platform specifics
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  if (NOT HAS_CUSTOM_LIBETPAN)
    find_package(ZLIB REQUIRED)
    find_library(ICONV_LIBRARY iconv REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
    target_link_libraries(nmail PUBLIC ${ZLIB_LIBRARIES} "${ICONV_LIBRARY}" "${COREFOUNDATION_LIBRARY}" "${SECURITY_LIBRARY}")

    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
    check_include_files("CFNetwork/CFNetwork.h" HAVE_CFNETWORK LANGUAGE C)
    if (HAVE_CFNETWORK)
      find_library(CFNETWORK_LIBRARY CFNetwork REQUIRED)
      target_link_libraries(nmail PUBLIC "${CFNETWORK_LIBRARY}")
    endif()
  endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
  find_library(WORDEXP_LIBRARY android-wordexp)
  target_link_libraries(nmail PUBLIC ${WORDEXP_LIBRARY})
endif()

# Includes
target_include_directories(nmail PRIVATE ${LIBETPAN_INCLUDE_DIR} ${XAPIAN_INCLUDE_DIR}
                           ${MAGIC_HEADERS} ${CYRUS_SASL_INCLUDE_DIR} ${CURSES_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR}
                           "ext/sqlite_modern_cpp/hdr" "ext/cereal/include" ${LIBUUID_HEADERS}
                           "ext/cyrus-imap/lib")

# Linking
target_link_libraries(nmail PUBLIC
                      ${CURSES_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${SQLite3_LIBRARIES}
                      ${XAPIAN_LIBRARIES} ${LIBETPAN_LIBRARY} ${CYRUS_SASL_LIBRARY}
                      ${MAGIC_LIBRARY} ${LIBUUID_LIBRARIES}
                      pthread ${CMAKE_DL_LIBS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(nmail PUBLIC -rdynamic)
endif()

# Manual
install(FILES src/nmail.1 DESTINATION share/man/man1)

# Utils
configure_file(src/oauth2nmail ${CMAKE_CURRENT_BINARY_DIR}/oauth2nmail COPYONLY)
configure_file(src/html2nmail ${CMAKE_CURRENT_BINARY_DIR}/html2nmail COPYONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/oauth2nmail DESTINATION bin)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/html2nmail DESTINATION bin)

# Themes
macro(add_theme themename)
  configure_file(themes/${themename} ${CMAKE_CURRENT_BINARY_DIR}/share/nmail/themes/${themename} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/share/nmail/themes/${themename} DESTINATION share/nmail/themes)
endmacro()
add_theme("default.conf")
add_theme("catppuccin-macchiato.conf")

# Uninstall
add_custom_target(uninstall
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/share/man/man1/nmail.1"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/oauth2nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/html2nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/share/nmail"
)
