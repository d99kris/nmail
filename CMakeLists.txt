# Project
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(nmail VERSION "1.33" LANGUAGES CXX)
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

# Application
add_executable(nmail
  ext/apathy/path.hpp
  ext/cxx-prettyprint/prettyprint.hpp
  src/addressbook.cpp
  src/addressbook.h
  src/aes.cpp
  src/aes.h
  src/body.cpp
  src/body.h
  src/config.cpp
  src/config.h
  src/contact.cpp
  src/contact.h
  src/flag.cpp
  src/flag.h
  src/header.cpp
  src/header.h
  src/imap.cpp
  src/imap.h
  src/imapmanager.cpp
  src/imapmanager.h
  src/lockfile.cpp
  src/lockfile.h
  src/log.cpp
  src/log.h
  src/loghelp.cpp
  src/loghelp.h
  src/main.cpp
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
  target_link_libraries(nmail PUBLIC sasl2 iconv z "-framework CoreFoundation" "-framework Security" "-framework CFNetwork")
endif()

# Headers
target_include_directories(nmail PRIVATE "ext")

# Compiler flags
set_target_properties(nmail PROPERTIES COMPILE_FLAGS
                      "-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith \
                       -Wcast-qual -Wno-missing-braces -Wswitch-default -Wunreachable-code \
                       -Wundef -Wuninitialized -Wcast-align \
                       -Wno-gnu-zero-variadic-macro-arguments")
target_compile_definitions(nmail PRIVATE PROJECT_VERSION="${PROJECT_VERSION}" LIBETPAN_IOS_DISABLE_SSL=0 WIN32=0)

# Dependency ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

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
  message(STATUS "Found libetpan")
endif()

include_directories(${LIBETPAN_INCLUDE_DIR})

# Linking
target_link_libraries(nmail PUBLIC ${CURSES_NCURSES_LIBRARY} OpenSSL::SSL ${LIBETPAN_LIBRARY} pthread ${CMAKE_DL_LIBS})

# Manual
install(FILES src/nmail.1 DESTINATION share/man/man1)

# Uninstall
add_custom_target(uninstall
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/share/man/man1/nmail.1"
)
