# build libgtest
add_library (gtest STATIC 
             ${GTEST_SOURCE_DIR}/gtest-all.cc 
             ${GTEST_SOURCE_DIR}/gtest_main.cc)
set_target_properties (gtest PROPERTIES INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} ${GTEST_INCLUDE_DIR})
set_target_properties (gtest PROPERTIES COMPILE_FLAGS ${COMPILE_FLAGS} -w)

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g ${CC_WARNING_ARGS}")

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
                    DEPENDS ${CMAKE_SOURCE_DIR}/data/com.canonical.indicator.datetime.gschema.xml
                    COMMAND cp -f ${CMAKE_SOURCE_DIR}/data/*gschema.xml ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})

# look for headers in our src dir, and also in the directories where we autogenerate files...
include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${DBUSTEST_INCLUDE_DIRS})

add_definitions (-DSANDBOX="${CMAKE_CURRENT_BINARY_DIR}")

# test-timezone-file
set (TEST_NAME test-timezone-file)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-timezone-geoclue
set (TEST_NAME test-timezone-geoclue)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${DBUSTEST_LIBRARIES} ${GTEST_LIBS})

# test-timezones
set (TEST_NAME test-timezones)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${DBUSTEST_LIBRARIES} ${GTEST_LIBS})

# test-clock
set (TEST_NAME test-clock)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-formatter
set (TEST_NAME test-formatter)
add_executable (${TEST_NAME} test-formatter.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-planner
set (TEST_NAME test-planner)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-locations
set (TEST_NAME test-locations)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-actions
set (TEST_NAME test-actions)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-live-actions
set (TEST_NAME test-live-actions)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-menus
set (TEST_NAME test-menus)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-utils
set (TEST_NAME test-utils)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-settings
set (TEST_NAME test-settings)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-exporter
set (TEST_NAME test-exporter)
add_executable (${TEST_NAME} ${TEST_NAME}.cc)
add_test (${TEST_NAME} ${TEST_NAME})
add_dependencies (${TEST_NAME} libindicatordatetimeservice)
target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

# test-desktop
#set (TEST_NAME test-desktop)
#add_executable (${TEST_NAME} ${TEST_NAME}.cc)
#add_test (${TEST_NAME} ${TEST_NAME})
#add_dependencies (${TEST_NAME} libindicatordatetimeservice)
#target_link_libraries (${TEST_NAME} indicatordatetimeservice gtest ${SERVICE_DEPS_LIBRARIES} ${GTEST_LIBS})

