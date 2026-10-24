# Project
cmake_minimum_required(VERSION 3.14...3.22 FATAL_ERROR)
project(nmail LANGUAGES CXX C)
include(CheckIncludeFile)
set(CMAKE_CXX_STANDARD 17)
message(STATUS "Using cmake ${CMAKE_VERSION}")

# Modules
include(CheckIncludeFiles)

# Install dirs
if(NOT DEFINED CMAKE_INSTALL_DATADIR)
  set(CMAKE_INSTALL_DATADIR "share")
endif()
if(NOT DEFINED CMAKE_INSTALL_MANDIR)
  set(CMAKE_INSTALL_MANDIR "${CMAKE_INSTALL_DATADIR}/man")
endif()

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

# Build origin (github / local, or packager-provided e.g. debian via
# -DNMAIL_BUILD_ORIGIN=debian). Auto-detected as "github" under GitHub Actions.
if(NOT DEFINED NMAIL_BUILD_ORIGIN)
  if(DEFINED ENV{GITHUB_ACTIONS})
    set(NMAIL_BUILD_ORIGIN "github")
  else()
    set(NMAIL_BUILD_ORIGIN "local")
  endif()
endif()
message(STATUS "Build origin: ${NMAIL_BUILD_ORIGIN}")

# Build git sha/branch, resolved at configure time from the source checkout.
# Guarded on a top-level .git so source-tarball builds (no repo) resolve to sha
# "unknown" / empty branch, without picking up an enclosing repo if the tarball
# is unpacked inside one.
set(NMAIL_BUILD_GIT_SHA "unknown")
set(NMAIL_BUILD_GIT_BRANCH "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  # Short sha, fixed to 7 chars (git's --short auto-grows the abbreviation to
  # stay unique, so a plain --short can yield 8+ chars in larger clones such as
  # GitHub's). Suffixed "+" when tracked files have local changes.
  execute_process(COMMAND git rev-parse --short=7 HEAD
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_SHA
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  if(GIT_SHA)
    execute_process(COMMAND git status --porcelain --untracked-files=no
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_DIRTY
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(GIT_DIRTY)
      set(GIT_SHA "${GIT_SHA}+")
    endif()
    set(NMAIL_BUILD_GIT_SHA "${GIT_SHA}")
  endif()

  # Branch name, left empty for detached HEAD (e.g. release-tag builds) so it is
  # only appended to the app name for actual working branches.
  execute_process(COMMAND git rev-parse --abbrev-ref HEAD
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_BRANCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  if(GIT_BRANCH AND NOT GIT_BRANCH STREQUAL "HEAD")
    set(NMAIL_BUILD_GIT_BRANCH "${GIT_BRANCH}")
  endif()
endif()
message(STATUS "Build git sha: ${NMAIL_BUILD_GIT_SHA}")
message(STATUS "Build git branch: ${NMAIL_BUILD_GIT_BRANCH}")

# Config - Static linking of external library dependencies
option(HAS_STATIC_EXTLIBS "Static Ext Libs" OFF)
message(STATUS "Static Ext Libs: ${HAS_STATIC_EXTLIBS}")
if(HAS_STATIC_EXTLIBS)
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # Fully-static is impossible on macOS: prefer static (.a) from the deps
    # prefix but fall back to the system dylibs/stubs (.tbd/.dylib) for the
    # libraries that only ship dynamically there (curl, expat, iconv). The
    # deps prefix is on CMAKE_PREFIX_PATH and searched before the SDK, so a
    # source-built .a wins wherever one exists; build-macos.sh's otool
    # allowlist enforces that only /usr/lib + frameworks stay dynamic.
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .tbd .dylib)
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    add_link_options(-static-libstdc++ -static-libgcc)
  endif()
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

# Feature - Core dump
option(HAS_COREDUMP "Core Dump" ON)
message(STATUS "Core Dump: ${HAS_COREDUMP}")

# Embedded helper scripts - bundled in the executable and extracted to the temp
# dir at runtime, so no separate helper scripts need to be installed.
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OAUTH2NMAIL_HEADER "${GENERATED_DIR}/oauth2nmailscript.h")
set(HTML2NMAIL_HEADER "${GENERATED_DIR}/html2nmailscript.h")

# Volatile build metadata (origin, git sha, git branch) is written to a
# generated buildinfo.h and included only from the two source files that consume
# it (src/util.cpp, src/version.cpp), rather than passed target-wide via
# target_compile_definitions() below. The git sha/branch change on every commit,
# and a target-wide define would recompile all of src/ each time; configure_file
# only rewrites the header when a value actually changes, so an unchanged sha
# is a no-op and a changed one recompiles just those two files.
set(BUILDINFO_HEADER "${GENERATED_DIR}/buildinfo.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.h.in"
               "${BUILDINFO_HEADER}" @ONLY)
add_custom_command(
  OUTPUT "${OAUTH2NMAIL_HEADER}"
  COMMAND "${CMAKE_COMMAND}"
          -DIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/oauth2nmail
          -DOUT_FILE=${OAUTH2NMAIL_HEADER}
          -DVAR_NAME=Oauth2nmailScript
          -P ${CMAKE_CURRENT_SOURCE_DIR}/utils/embedfile.cmake
  DEPENDS src/oauth2nmail utils/embedfile.cmake
  COMMENT "Embedding oauth2nmail script"
  VERBATIM
)
add_custom_command(
  OUTPUT "${HTML2NMAIL_HEADER}"
  COMMAND "${CMAKE_COMMAND}"
          -DIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/html2nmail
          -DOUT_FILE=${HTML2NMAIL_HEADER}
          -DVAR_NAME=Html2nmailScript
          -P ${CMAKE_CURRENT_SOURCE_DIR}/utils/embedfile.cmake
  DEPENDS src/html2nmail utils/embedfile.cmake
  COMMENT "Embedding html2nmail script"
  VERBATIM
)

# Application
add_executable(nmail
  ${OAUTH2NMAIL_HEADER}
  ${HTML2NMAIL_HEADER}
  ${BUILDINFO_HEADER}
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
  src/imaputil.cpp
  src/imaputil.h
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
  src/uuid.cpp
  src/uuid.h
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

# Detached debug symbols (enabled by the dist/release build scripts). Compile
# nmail's own translation units with -g1 so the DWARF can be split into a
# detached nmail.debug / .dSYM after the build. Two deliberate choices keep the
# artifacts small while still symbolicating crash logs:
#  * -g1 (not the default -g2): emits function descriptions and line tables --
#    enough to resolve a backtrace to function + file:line -- but omits the
#    local-variable and full-type DWARF that dominates -g2's size (~4x smaller
#    here). The trade-off is no rich interactive variable inspection.
#  * scoped to the nmail target: ext/libetpan is left without debug info, so only
#    nmail's own frames symbolicate. nmail's C++ TUs -- not libetpan's C -- are
#    what dominate the DWARF (heavy templated headers: cereal, sqlite_modern_cpp,
#    the STL), so libetpan is only a small slice anyway.
# -g1 does not change codegen, so the stripped release binary stays byte-for-byte
# a plain Release build.
option(HAS_DEBUG_SYMBOLS "Emit detached debug symbols for nmail" OFF)
message(STATUS "Debug symbols: ${HAS_DEBUG_SYMBOLS}")
if(HAS_DEBUG_SYMBOLS)
  target_compile_options(nmail PRIVATE -g1)
endif()

# Features
if(HAS_COREDUMP)
  target_compile_definitions(nmail PRIVATE HAS_COREDUMP="${HAS_COREDUMP}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # Core dump entitlements
    set(SIGNSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/utils/sign")
    set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nmail.entitlements")
    install(CODE "execute_process(COMMAND
      \"${SIGNSCRIPT}\" \"${ENTITLEMENTS}\" \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/nmail\"
      )" COMPONENT Runtime)
  endif()
endif()

# Build info (build type, external linkage), logged at startup via
# Util::GetBuildInfo(). Build type maps CMAKE_BUILD_TYPE to a single define.
# The volatile origin/git-sha/git-branch values are supplied instead via the
# generated buildinfo.h (see configure_file above) so they don't recompile the
# whole target on every commit.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(nmail PRIVATE NMAIL_BUILD_DEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_definitions(nmail PRIVATE NMAIL_BUILD_RELWITHDEBINFO=1)
else()
  target_compile_definitions(nmail PRIVATE NMAIL_BUILD_RELEASE=1)
endif()
if(HAS_STATIC_EXTLIBS)
  target_compile_definitions(nmail PRIVATE NMAIL_BUILD_STATIC_EXTLIBS=1)
endif()
# musl provides no C preprocessor version macro, so detect it from the compiler
# target triple at configure time and expose it for startup libc labeling.
execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
                OUTPUT_VARIABLE TARGET_TRIPLE ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(TARGET_TRIPLE MATCHES "musl")
  target_compile_definitions(nmail PRIVATE NMAIL_BUILD_MUSL=1)
  # musl has no execinfo/backtrace() and libgcc's _Unwind_Backtrace cannot step
  # past musl's signal trampoline, so the crash handler walks the frame-pointer
  # chain seeded from the signal ucontext (see Util::SignalCrashHandler /
  # FramePointerBacktrace in src/util.cpp). That walk needs a maintained frame
  # pointer in nmail's own frames, which -O2 otherwise omits. Scoped to the nmail
  # target (not the static deps) and to musl only -- the glibc/macOS builds use
  # backtrace() and are unaffected. Paired with -no-pie on the musl link line
  # (utils/dist/build-in-container.sh) so the logged addresses are absolute and
  # resolve directly with `addr2line -e nmail.debug`.
  target_compile_options(nmail PRIVATE -fno-omit-frame-pointer)
endif()

# Dependency ncurses
if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  target_compile_definitions(nmail PRIVATE _XOPEN_SOURCE_EXTENDED)
  target_link_libraries(nmail PUBLIC ncurses)
else()
  set(CURSES_NEED_NCURSES TRUE)
  set(CURSES_NEED_WIDE TRUE)
  if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    # FreeBSD base ncurses is wide-char but has no ncursesw/ header dir, which
    # FindCurses requires with CURSES_NEED_WIDE, so point it at the base headers.
    # Its ncurses.h only declares the wide-char API with _XOPEN_SOURCE_EXTENDED.
    find_path(CURSES_INCLUDE_PATH NAMES ncurses.h PATHS /usr/include NO_DEFAULT_PATH)
    target_compile_definitions(nmail PRIVATE _XOPEN_SOURCE_EXTENDED)
  endif()
  find_package(Curses REQUIRED)
  target_compile_options(nmail PUBLIC ${NCURSES_CFLAGS})
endif()

# Dependency openssl
find_package(OpenSSL REQUIRED)

# Dependency xapian
find_package(Xapian REQUIRED)
if(HAS_STATIC_EXTLIBS)
  # static libxapian needs zlib after it on the link line
  find_package(ZLIB)
  if(ZLIB_FOUND)
    list(APPEND XAPIAN_LIBRARIES ${ZLIB_LIBRARIES})
  endif()
endif()

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
  find_library(EXECINFO_LIBRARY execinfo)
  if(EXECINFO_LIBRARY)
    target_link_libraries(nmail PUBLIC ${EXECINFO_LIBRARY})
  endif()
endif()

# Dependency magic
find_library(MAGIC_LIBRARY magic)
find_path(MAGIC_HEADERS magic.h)
if(NOT MAGIC_LIBRARY OR NOT MAGIC_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libmagic")
else()
  message(STATUS "Found libmagic: ${MAGIC_LIBRARY} and ${MAGIC_HEADERS}")
endif()
if(HAS_STATIC_EXTLIBS)
  # static libmagic needs its compression backends
  foreach(MAGIC_DEP z bz2 lzma zstd)
    find_library(MAGIC_DEP_LIBRARY_${MAGIC_DEP} ${MAGIC_DEP})
    if(MAGIC_DEP_LIBRARY_${MAGIC_DEP})
      list(APPEND MAGIC_LIBRARY ${MAGIC_DEP_LIBRARY_${MAGIC_DEP}})
    endif()
  endforeach()
endif()

# Dependency libuuid
find_library(LIBUUID_LIBRARIES NAMES uuid)
find_path(LIBUUID_HEADERS uuid.h PATH_SUFFIXES uuid/)
if(NOT LIBUUID_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libuuid headers")
endif()
# On macOS the uuid_generate/uuid_unparse symbols live in libSystem and the
# <uuid/uuid.h> header ships in the SDK, so no standalone libuuid is required
# (a static deps build in particular need not ship one). On OpenBSD and FreeBSD
# the BSD uuid_create/uuid_to_string API lives in libc with <uuid.h>; avoid
# ossp-uuid due to uuid_create symbol clashes with libc's and breaks Xapian.
if(NOT LIBUUID_LIBRARIES AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin|OpenBSD|FreeBSD")
  message(FATAL_ERROR "ERROR: Could not find libuuid")
endif()
# When no standalone libuuid exists (the macOS case above), find_library leaves
# LIBUUID_LIBRARIES as the literal "LIBUUID_LIBRARIES-NOTFOUND"; scrub it to
# empty so it does not land on the target link line below as a bogus library.
if(NOT LIBUUID_LIBRARIES)
  set(LIBUUID_LIBRARIES "")
endif()
message(STATUS "Found libuuid: ${LIBUUID_LIBRARIES} and ${LIBUUID_HEADERS}")

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
target_include_directories(nmail PRIVATE
                           ${LIBETPAN_INCLUDE_DIR} ${XAPIAN_INCLUDE_DIR} ${MAGIC_HEADERS}
                           ${CYRUS_SASL_INCLUDE_DIR} ${CURSES_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR}
                           ${LIBUUID_HEADERS}
                           "${GENERATED_DIR}"
                           "ext/cereal/include" "ext/cyrus-imap/lib" "ext/sqlite_modern_cpp/hdr"
                           "ext/utfcpp/source")

# Linking
target_link_libraries(nmail PUBLIC
                      ${CURSES_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${SQLite3_LIBRARIES}
                      ${XAPIAN_LIBRARIES} ${LIBETPAN_LIBRARY} ${CYRUS_SASL_LIBRARY}
                      ${MAGIC_LIBRARY} ${LIBUUID_LIBRARIES}
                      pthread ${CMAKE_DL_LIBS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(nmail PUBLIC -rdynamic)
endif()

# Manual
install(FILES src/nmail.1 DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

# Third-party license attribution
#
# Every nmail binary links Xapian -- making it a combined work under the GNU
# GPL v2 -- and compiles in the third-party sources vendored under ext/, so the
# notices are installed for every build, not just the static tarballs produced
# by utils/dist/build-*.sh. The combined THIRD_PARTY_LICENSES file is assembled
# here from fragments: a preamble carrying the combined-work statement and the
# full GPL v2 text, the always-applicable ext/ notices, and -- for static builds
# only -- the notices for the system libraries linked into the binary. Both
# fragments are hand-maintained; keep THIRD_PARTY_LICENSES.static in sync with
# the versions pinned in utils/dist/Dockerfile.* and
# utils/dist/build-deps-macos.sh.
set(TPL_FRAGMENTS
  "${CMAKE_CURRENT_SOURCE_DIR}/utils/licenses/preamble.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/ext/THIRD_PARTY_LICENSES")
if(HAS_STATIC_EXTLIBS)
  list(APPEND TPL_FRAGMENTS
       "${CMAKE_CURRENT_SOURCE_DIR}/utils/dist/THIRD_PARTY_LICENSES.static")
endif()

set(TPL_COMBINED "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_LICENSES")
file(WRITE "${TPL_COMBINED}" "")
foreach(TPL_FRAGMENT ${TPL_FRAGMENTS})
  if(NOT EXISTS "${TPL_FRAGMENT}")
    message(FATAL_ERROR "Missing third-party license fragment: ${TPL_FRAGMENT}")
  endif()
  file(READ "${TPL_FRAGMENT}" TPL_FRAGMENT_TEXT)
  file(APPEND "${TPL_COMBINED}" "${TPL_FRAGMENT_TEXT}")
  file(APPEND "${TPL_COMBINED}" "\n")
  # Regenerate the combined file when a fragment changes.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TPL_FRAGMENT}")
endforeach()

# Install the combined notices next to nmail's own LICENSE under the FHS doc
# dir, so every installed binary carries the required attribution; package.sh
# also surfaces both files at the tarball top level.
install(FILES "${TPL_COMBINED}" "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/nmail")

# Uninstall
add_custom_target(uninstall
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/nmail"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1/nmail.1"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/doc/nmail/THIRD_PARTY_LICENSES"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/doc/nmail/LICENSE"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/doc/nmail"
)
