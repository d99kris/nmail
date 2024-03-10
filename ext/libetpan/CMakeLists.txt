# Project
cmake_minimum_required(VERSION 3.14...3.22 FATAL_ERROR)
project(libetpan LANGUAGES C)
if(LIBETPAN_LIBNAME)
  set(LIBNAME ${LIBETPAN_LIBNAME})
  message(STATUS "Libetpan custom lib name: ${LIBNAME}")
else()
  set(LIBNAME etpan)
  message(STATUS "Libetpan default lib name: ${LIBNAME}")
endif()

# Modules
include(CheckIncludeFiles)

# Functions
function(check_include_add_define header define loglevel)
  set(CMAKE_REQUIRED_INCLUDES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
  check_include_files(${header} ${define} LANGUAGE C)
  if(${define})
    target_compile_definitions(${LIBNAME} PRIVATE ${define}=1)
  else()
    if (NOT loglevel STREQUAL "NOTICE")
      message(${loglevel} "Header file ${header} not found, this may result in an incomplete build.")
    endif()
  endif()
endfunction()

# Ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Paths
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Sources
set(LIB_SOURCES
  src/low-level/imap/annotatemore_parser.c
  src/low-level/imap/mailimap_sort.c
  src/low-level/imap/xgmthrid.c
  src/low-level/imap/namespace_parser.c
  src/low-level/imap/annotatemore_sender.c
  src/low-level/imap/mailimap_sort_types.c
  src/low-level/imap/mailimap_socket.c
  src/low-level/imap/qresync.c
  src/low-level/imap/annotatemore.c
  src/low-level/imap/condstore.c
  src/low-level/imap/annotatemore_types.c
  src/low-level/imap/mailimap_extension.c
  src/low-level/imap/namespace_sender.c
  src/low-level/imap/mailimap_oauth2.c
  src/low-level/imap/xgmlabels.c
  src/low-level/imap/quota_types.c
  src/low-level/imap/clientid_sender.c
  src/low-level/imap/qresync_types.c
  src/low-level/imap/clientid.c
  src/low-level/imap/acl_parser.c
  src/low-level/imap/uidplus_parser.c
  $<$<CONFIG:Debug>:src/low-level/imap/mailimap_print.c>
  src/low-level/imap/mailimap.c
  src/low-level/imap/uidplus_sender.c
  src/low-level/imap/acl_sender.c
  src/low-level/imap/mailimap_types_helper.c
  src/low-level/imap/quota.c
  src/low-level/imap/condstore_types.c
  src/low-level/imap/quota_sender.c
  src/low-level/imap/mailimap_parser.c
  src/low-level/imap/uidplus.c
  src/low-level/imap/quota_parser.c
  src/low-level/imap/xlist.c
  src/low-level/imap/mailimap_sender.c
  src/low-level/imap/mailimap_ssl.c
  src/low-level/imap/acl_types.c
  src/low-level/imap/acl.c
  src/low-level/imap/mailimap_compress.c
  src/low-level/imap/namespace_types.c
  src/low-level/imap/mailimap_helper.c
  src/low-level/imap/mailimap_keywords.c
  src/low-level/imap/uidplus_types.c
  src/low-level/imap/mailimap_id.c
  src/low-level/imap/enable.c
  src/low-level/imap/idle.c
  src/low-level/imap/mailimap_id_sender.c
  src/low-level/imap/xgmmsgid.c
  src/low-level/imap/mailimap_types.c
  src/low-level/imap/mailimap_id_parser.c
  src/low-level/imap/namespace.c
  src/low-level/imap/mailimap_id_types.c
  src/low-level/mh/mailmh.c
  src/low-level/mbox/mailmbox_types.c
  src/low-level/mbox/mailmbox_parse.c
  src/low-level/mbox/mailmbox.c
  src/low-level/imf/mailimf_write_generic.c
  src/low-level/imf/mailimf_write_file.c
  src/low-level/imf/mailimf.c
  src/low-level/imf/mailimf_types.c
  src/low-level/imf/mailimf_write_mem.c
  src/low-level/imf/mailimf_types_helper.c
  src/low-level/feed/parser_rss20.c
  src/low-level/feed/newsfeed_private.c
  src/low-level/feed/newsfeed_item.c
  src/low-level/feed/newsfeed_item_enclosure.c
  src/low-level/feed/parser_rdf.c
  src/low-level/feed/parser_atom10.c
  src/low-level/feed/parser_atom03.c
  src/low-level/feed/date.c
  src/low-level/feed/parser.c
  src/low-level/feed/newsfeed.c
  src/low-level/smtp/mailsmtp_socket.c
  src/low-level/smtp/mailsmtp_oauth2.c
  src/low-level/smtp/mailsmtp.c
  src/low-level/smtp/mailsmtp_ssl.c
  src/low-level/smtp/mailsmtp_helper.c
  src/low-level/pop3/mailpop3_ssl.c
  src/low-level/pop3/mailpop3_socket.c
  src/low-level/pop3/mailpop3.c
  src/low-level/pop3/mailpop3_helper.c
  src/low-level/mime/mailmime_write_generic.c
  src/low-level/mime/mailmime.c
  src/low-level/mime/mailmime_write_file.c
  src/low-level/mime/mailmime_decode.c
  src/low-level/mime/mailmime_types.c
  src/low-level/mime/mailmime_content.c
  src/low-level/mime/mailmime_disposition.c
  src/low-level/mime/mailmime_write_mem.c
  src/low-level/mime/mailmime_types_helper.c
  src/low-level/maildir/maildir.c
  src/low-level/nntp/newsnntp_ssl.c
  src/low-level/nntp/newsnntp_socket.c
  src/low-level/nntp/newsnntp.c
  src/data-types/charconv.c
  src/data-types/mailsasl.c
  src/data-types/mail_cache_db.c
  src/data-types/mailstream_helper.c
  src/data-types/mailstream_low.c
  src/data-types/mailstream_compress.c
  src/data-types/maillock.c
  src/data-types/mailstream_cancel.c
  src/data-types/base64.c
  src/data-types/clist.c
  src/data-types/mmapstring.c
  src/data-types/connect.c
  src/data-types/mailstream.c
  src/data-types/md5.c
  src/data-types/carray.c
  src/data-types/chash.c
  src/data-types/mailstream_socket.c
  src/data-types/mailstream_ssl.c
  src/data-types/timeutils.c
  src/data-types/mailsem.c
  src/data-types/mailstream_cfstream.c
  src/driver/interface/mailmessage.c
  src/driver/interface/maildriver_types.c
  src/driver/interface/mailmessage_types.c
  src/driver/interface/mailfolder.c
  src/driver/interface/mailstorage_tools.c
  src/driver/interface/maildriver_types_helper.c
  src/driver/interface/mailmessage_tools.c
  src/driver/interface/maildriver_tools.c
  src/driver/interface/maildriver.c
  src/driver/interface/mailstorage.c
  src/driver/tools/mailthread_types.c
  src/driver/tools/generic_cache.c
  src/driver/tools/mailthread.c
  src/driver/tools/imfcache.c
  src/driver/implementation/data-message/data_message_driver.c
  src/driver/implementation/imap/imapdriver_cached.c
  src/driver/implementation/imap/imapdriver_message.c
  src/driver/implementation/imap/imapdriver_cached_message.c
  src/driver/implementation/imap/imapdriver_tools.c
  src/driver/implementation/imap/imapstorage.c
  src/driver/implementation/imap/imapdriver.c
  src/driver/implementation/mh/mhdriver_message.c
  src/driver/implementation/mh/mhdriver_cached_message.c
  src/driver/implementation/mh/mhdriver_cached.c
  src/driver/implementation/mh/mhdriver_tools.c
  src/driver/implementation/mh/mhstorage.c
  src/driver/implementation/mh/mhdriver.c
  src/driver/implementation/mbox/mboxstorage.c
  src/driver/implementation/mbox/mboxdriver.c
  src/driver/implementation/mbox/mboxdriver_message.c
  src/driver/implementation/mbox/mboxdriver_cached.c
  src/driver/implementation/mbox/mboxdriver_tools.c
  src/driver/implementation/mbox/mboxdriver_cached_message.c
  src/driver/implementation/feed/feeddriver_message.c
  src/driver/implementation/feed/feedstorage.c
  src/driver/implementation/feed/feeddriver.c
  src/driver/implementation/pop3/pop3driver_cached.c
  src/driver/implementation/pop3/pop3driver.c
  src/driver/implementation/pop3/pop3driver_cached_message.c
  src/driver/implementation/pop3/pop3driver_message.c
  src/driver/implementation/pop3/pop3storage.c
  src/driver/implementation/pop3/pop3driver_tools.c
  src/driver/implementation/db/dbdriver.c
  src/driver/implementation/db/dbstorage.c
  src/driver/implementation/db/dbdriver_message.c
  src/driver/implementation/maildir/maildirdriver.c
  src/driver/implementation/maildir/maildirdriver_cached_message.c
  src/driver/implementation/maildir/maildirstorage.c
  src/driver/implementation/maildir/maildirdriver_tools.c
  src/driver/implementation/maildir/maildirdriver_message.c
  src/driver/implementation/maildir/maildirdriver_cached.c
  src/driver/implementation/nntp/nntpdriver.c
  src/driver/implementation/nntp/nntpdriver_cached_message.c
  src/driver/implementation/nntp/nntpstorage.c
  src/driver/implementation/nntp/nntpdriver_cached.c
  src/driver/implementation/nntp/nntpdriver_message.c
  src/driver/implementation/nntp/nntpdriver_tools.c
  src/driver/implementation/hotmail/hotmailstorage.c
  src/driver/implementation/mime-message/mime_message_driver.c
  src/main/libetpan_version.c
  src/engine/mailprivacy.c
  src/engine/mailprivacy_tools.c
  src/engine/mailengine.c
  src/engine/mailprivacy_gnupg.c
  src/engine/mailprivacy_smime.c
)

if(LIBETPAN_STATIC)
  add_library(${LIBNAME} STATIC ${LIB_SOURCES})
  message(STATUS "Libetpan: static")
else()
  add_library(${LIBNAME} SHARED ${LIB_SOURCES})
  message(STATUS "Libetpan: shared")
endif()

# Sources (Windows)
if(WIN32)
  target_sources(${LIBNAME} PRIVATE
    src/bsd/getopt_long.c
    src/bsd/getopt.c
    src/windows/mmap.c
    src/windows/inet_aton.c
    src/windows/time_r.c
    src/windows/misc.c
    src/windows/dirent.c
  )
endif()

# Headers
add_custom_target(libetpan_headers
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND mkdir -p                                ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan
    COMMAND cp -pPR src/low-level/imap/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/mh/*.h              ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/mbox/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/imf/*.h             ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/feed/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/smtp/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/pop3/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/mime/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/maildir/*.h         ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/low-level/nntp/*.h            ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/data-types/*.h                ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/driver/interface/*.h          ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/driver/tools/*.h              ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/main/*.h                      ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/engine/*.h                    ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR src/driver/implementation/*/*.h   ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMAND cp -pPR build-cmake/*.h                   ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan/
    COMMENT "Copy headers"
)
add_dependencies(${LIBNAME} libetpan_headers)
target_include_directories(${LIBNAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(${LIBNAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include/libetpan)

# Headers - Standard
check_include_add_define("arpa/inet.h" HAVE_ARPA_INET_H WARNING)
check_include_add_define("ctype.h" HAVE_CTYPE_H WARNING)
check_include_add_define("dlfcn.h" HAVE_DLFCN_H WARNING)
check_include_add_define("fcntl.h" HAVE_FCNTL_H WARNING)
check_include_add_define("iconv.h" HAVE_ICONV WARNING)
check_include_add_define("inttypes.h" HAVE_INTTYPES_H WARNING)
check_include_add_define("limits.h" HAVE_LIMITS_H WARNING)
check_include_add_define("memory.h" HAVE_MEMORY_H WARNING)
check_include_add_define("netinet/in.h" HAVE_NETINET_IN_H WARNING)
check_include_add_define("pthread.h" HAVE_PTHREAD_H WARNING)
check_include_add_define("stdint.h" HAVE_STDINT_H WARNING)
check_include_add_define("stdlib.h" HAVE_STDLIB_H WARNING)
check_include_add_define("string.h" HAVE_STRING_H WARNING)
check_include_add_define("strings.h" HAVE_STRINGS_H WARNING)
check_include_add_define("sys/mman.h" HAVE_SYS_MMAN_H WARNING)
check_include_add_define("sys/param.h" HAVE_SYS_PARAM_H WARNING)
check_include_add_define("sys/poll.h" HAVE_SYS_POLL_H WARNING)
check_include_add_define("sys/select.h" HAVE_SELECT_H WARNING)
check_include_add_define("sys/select.h" HAVE_SYS_SELECT_H WARNING)
check_include_add_define("sys/socket.h" HAVE_SYS_SOCKET_H WARNING)
check_include_add_define("sys/stat.h" HAVE_SYS_STAT_H WARNING)
check_include_add_define("sys/types.h" HAVE_SYS_TYPES_H WARNING)
check_include_add_define("sys/uio.h" HAVE_SYS_UIO_H WARNING)
check_include_add_define("unistd.h" HAVE_UNISTD_H WARNING)

# Headers - Others (Disabled)
#check_include_add_define("lmdb.h" HAVE_LMDB_H NOTICE)
#check_include_add_define("CFNetwork/CFNetwork.h" HAVE_CFNETWORK NOTICE)

# Capabilities - Required
target_compile_definitions(${LIBNAME} PRIVATE HAVE___ATTRIBUTE__=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_CURL=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_EXPAT=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_GETADDRINFO=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_GETNAMEINFO=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_GETOPT_LONG=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_IPV6=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_SETENV=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_SOCKLEN_T=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_STRUCT_SOCKADDR_STORAGE=1)
target_compile_definitions(${LIBNAME} PRIVATE HAVE_ZLIB=1)
target_compile_definitions(${LIBNAME} PRIVATE LIBETPAN_REENTRANT=1)
target_compile_definitions(${LIBNAME} PRIVATE STDC_HEADERS=1)
target_compile_definitions(${LIBNAME} PRIVATE USE_SASL=1)
target_compile_definitions(${LIBNAME} PRIVATE USE_SSL=1)
if(WIN32)
  target_compile_definitions(${LIBNAME} PRIVATE HAVE_MINGW32_SYSTEM=1)
  target_compile_definitions(${LIBNAME} PRIVATE HAVE_WINSOCK2_H=1)
endif()

# Capabilities - Optional
if(${HAVE_LMDB_H})
  target_compile_definitions(${LIBNAME} PRIVATE LMDB=1)
  target_sources(${LIBNAME} PRIVATE "src/data-types/mail_cache_lmdb.c")
endif()

# Capabilities - Disabled
#target_compile_definitions(${LIBNAME} PRIVATE HAVE_CONFIG_H=1)
#target_compile_definitions(${LIBNAME} PRIVATE HAVE_ICONV_PROTO_CONST=1)
#target_compile_definitions(${LIBNAME} PRIVATE USE_POLL=1)
#target_compile_definitions(${LIBNAME} PRIVATE UNSTRICT_SYNTAX=1)
#target_compile_definitions(${LIBNAME} PRIVATE USE_GNUTLS=1)
#target_compile_definitions(${LIBNAME} PRIVATE HAVE_LIBLOCKFILE=1)

# Find Dependencies - curl
find_package(CURL REQUIRED)
target_include_directories(${LIBNAME} PRIVATE ${CURL_INCLUDE_DIR})

# Find Dependencies - expat
find_package(EXPAT REQUIRED)
target_include_directories(${LIBNAME} PRIVATE ${EXPAT_INCLUDE_DIRS})

# Find Dependencies - OpenSSL
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  list(APPEND OPENSSL_ROOT_DIR /usr/local/opt/openssl)
  list(APPEND OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl)
endif()
find_package(OpenSSL REQUIRED)
target_include_directories(${LIBNAME} PRIVATE ${OPENSSL_INCLUDE_DIR})

# Find Dependencies - SASL2
find_library(CYRUS_SASL_LIBRARY sasl2 REQUIRED)
find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args(sasl2 DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
target_include_directories(${LIBNAME} PRIVATE ${CYRUS_SASL_INCLUDE_DIR})

# Find Dependencies - zlib
find_package(ZLIB REQUIRED)
target_include_directories(${LIBNAME} PRIVATE ${ZLIB_INCLUDE_DIRS})

# Compilation
target_compile_options(${LIBNAME} PRIVATE -w -Wall $<$<CONFIG:Debug>:-DDEBUG>)

# Linking
target_link_libraries(${LIBNAME} PUBLIC ${CURL_LIBRARIES} ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${CYRUS_SASL_LIBRARY})

# Linking (macOS)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_library(ICONV_LIBRARY iconv REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  find_library(SECURITY_LIBRARY Security REQUIRED)

  target_link_libraries(${LIBNAME} PUBLIC "${ICONV_LIBRARY}" "${COREFOUNDATION_LIBRARY}" "${SECURITY_LIBRARY}")
  if(${HAVE_CFNETWORK})
    find_library(CFNETWORK_LIBRARY CFNetwork REQUIRED)
    target_link_libraries(${LIBNAME} PUBLIC "${CFNETWORK_LIBRARY}")
  endif()
endif()

# Install
if(LIBETPAN_NOINSTALL)
  message(STATUS "Libetpan: skip install")
else()
  message(STATUS "Libetpan: install")
  install(TARGETS ${LIBNAME} DESTINATION lib)
endif()
