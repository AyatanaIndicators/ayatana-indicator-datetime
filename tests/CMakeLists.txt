# build libgtest
add_library (gtest STATIC 
             ${GTEST_SOURCE_DIR}/gtest-all.cc 
             ${GTEST_SOURCE_DIR}/gtest_main.cc)
set_target_properties (gtest PROPERTIES INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} ${GTEST_INCLUDE_DIR})
set_target_properties (gtest PROPERTIES COMPILE_FLAGS ${COMPILE_FLAGS} -w)

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ${CC_WARNING_ARGS}")

# dbustest
pkg_check_modules(DBUSTEST REQUIRED
                  dbustest-1>=14.04.0)
include_directories (SYSTEM ${DBUSTEST_INCLUDE_DIRS})

# build the necessary schemas
set_directory_properties (PROPERTIES
                          ADDITIONAL_MAKE_CLEAN_FILES gschemas.compiled)
set_source_files_properties (gschemas.compiled GENERATED)

# GSettings:
# compile the indicator-datetime schema into a gschemas.compiled file in this directory,
# and help the tests to find that file by setting -DSCHEMA_DIR
set (SCHEMA_DIR ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DSCHEMA_DIR="${SCHEMA_DIR}")
execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas
                 OUTPUT_VARIABLE COMPILE_SCHEMA_EXECUTABLE
                 OUTPUT_STRIP_TRAILING_WHITESPACE)
add_custom_command (OUTPUT gschemas.compiled
                    DEPENDS ${CMAKE_BINARY_DIR}/data/com.canonical.indicator.datetime.gschema.xml
                    COMMAND cp -f ${CMAKE_BINARY_DIR}/data/*gschema.xml ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})

# look for headers in our src dir, and also in the directories where we autogenerate files...
include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${CMAKE_BINARY_DIR}/src)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${DBUSTEST_INCLUDE_DIRS})


add_definitions (-DSANDBOX="${CMAKE_CURRENT_BINARY_DIR}")

function(add_test_by_name name)
  set (TEST_NAME ${name})
  add_executable (${TEST_NAME} ${TEST_NAME}.cpp gschemas.compiled)
  add_test (${TEST_NAME} ${TEST_NAME})
  target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${DBUSTEST_LIBRARIES} ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})
endfunction()
add_test_by_name(test-datetime)
add_test_by_name(test-snap)
add_test_by_name(test-actions)
add_test_by_name(test-alarm-queue)
add_test(NAME dear-reader-the-next-test-takes-60-seconds COMMAND true)
add_test_by_name(test-clock)
add_test_by_name(test-exporter)
add_test_by_name(test-formatter)
add_test_by_name(test-live-actions)
add_test_by_name(test-locations)
add_test_by_name(test-menus)
add_test_by_name(test-planner)
add_test_by_name(test-settings)
add_test_by_name(test-timezone-file)
add_test_by_name(test-utils)

set (TEST_NAME manual-test-snap)
add_executable (${TEST_NAME} ${TEST_NAME}.cpp)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

##
## EDS Tests
## 

find_program(DBUS_RUNNER dbus-test-runner)

function(add_eds_ics_test_by_name name)
  set (TEST_NAME ${name})
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp gschemas.compiled)
  target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${DBUSTEST_LIBRARIES} ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})
  add_test (${TEST_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/run-eds-ics-test.sh
            ${DBUS_RUNNER}                                         # arg1: dbus-test-runner exec
            ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}               # arg2: test executable path
            ${TEST_NAME}                                           # arg3: test name
            ${CMAKE_CURRENT_SOURCE_DIR}/test-eds-ics-config-files  # arg4: base directory for config file template
            ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.ics)          # arg5: the ical file for this test
endfunction()
add_eds_ics_test_by_name(test-eds-ics-all-day-events)
add_eds_ics_test_by_name(test-eds-ics-repeating-events)
add_eds_ics_test_by_name(test-eds-ics-nonrepeating-events)
add_eds_ics_test_by_name(test-eds-ics-repeating-valarms)
add_eds_ics_test_by_name(test-eds-ics-missing-trigger)
add_eds_ics_test_by_name(test-eds-ics-tzids)
add_eds_ics_test_by_name(test-eds-ics-tzids-2)


# disabling the timezone unit tests because they require
# https://code.launchpad.net/~ted/dbus-test-runner/multi-interface-test/+merge/199724
# which hasn't landed yet. These can be re-enabled as soon as that lands.
#function(add_dbusmock_test_by_name name)
#  set (TEST_NAME ${name})
#  add_executable (${TEST_NAME} ${TEST_NAME}.cpp gschemas.compiled)
#  add_test (${TEST_NAME} ${TEST_NAME})
#  target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${DBUSTEST_LIBRARIES} ${GTEST_LIBS})
#endfunction()
#add_dbusmock_test_by_name(test-timezone-geoclue)
#add_dbusmock_test_by_name(test-timezones)
