# Tests
#
# Unit tests for utility functions with self-contained logic, built as small
# executables and run by ctest (via ./make.sh tests).
#
# As nmail is built as a single executable, the sources needed by the tests are
# compiled into an object library here, using the build settings of the nmail
# target, and linked into each test. Keep the list to the sources needed, as
# they are compiled in addition to the regular build.

# Sources needed by tests
set(NMAILTEST_SOURCES
  ../src/apputil.cpp
  ../src/config.cpp
  ../src/extutil.cpp
  ../src/fileutil.cpp
  ../src/log.cpp
  ../src/strutil.cpp
  ../src/sysutil.cpp
  ../src/textutil.cpp
  ../src/textwrap.cpp
  ../src/version.cpp
)

# Build settings of nmail
get_target_property(NMAIL_COMPILE_FLAGS nmail COMPILE_FLAGS)
get_target_property(NMAIL_LINK_LIBRARIES nmail LINK_LIBRARIES)

# Macro applying build settings of nmail
macro(target_nmail_config targetname)
  target_compile_definitions(${targetname} PRIVATE $<TARGET_PROPERTY:nmail,COMPILE_DEFINITIONS>)
  target_compile_options(${targetname} PRIVATE $<TARGET_PROPERTY:nmail,COMPILE_OPTIONS>)
  target_include_directories(${targetname} PRIVATE $<TARGET_PROPERTY:nmail,INCLUDE_DIRECTORIES>)
  set_target_properties(${targetname} PROPERTIES COMPILE_FLAGS "${NMAIL_COMPILE_FLAGS}")
endmacro()

# Object library
add_library(nmailtest OBJECT ${NMAILTEST_SOURCES})
target_nmail_config(nmailtest)
add_dependencies(nmailtest nmail) # for generated headers

# Test macro add_unit_test
macro(add_unit_test testname)
  add_executable(${testname} ${testname}.cpp $<TARGET_OBJECTS:nmailtest>)

  # Headers and compiler flags
  target_nmail_config(${testname})
  target_include_directories(${testname} BEFORE PRIVATE "../src")

  # Linking
  target_link_libraries(${testname} PUBLIC ${NMAIL_LINK_LIBRARIES})

  # Test registration. The timeout ensures a hang in a tested function fails
  # the test run, rather than blocking it indefinitely.
  add_test(NAME ${testname} COMMAND ${testname})
  set_tests_properties(${testname} PROPERTIES TIMEOUT 60)
endmacro()

# Unit tests
add_unit_test(test001)
add_unit_test(test002)
add_unit_test(test003)
