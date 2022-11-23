project (ayatana-indicator-datetime C CXX)
cmake_minimum_required (VERSION 3.13)

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set (PROJECT_VERSION "22.9.1")
set (PACKAGE ${CMAKE_PROJECT_NAME})

# Options
option(ENABLE_TESTS "Enable all tests and checks" OFF)
option(ENABLE_COVERAGE "Enable coverage reports (includes enabling all tests and checks)" OFF)
option(ENABLE_WERROR "Treat all build warnings as errors" OFF)
option(ENABLE_LOMIRI_FEATURES "Build with Lomiri-specific libraries, schemas and media" OFF)

if(ENABLE_COVERAGE)
    set(ENABLE_TESTS ON)
    set(CMAKE_BUILD_TYPE "Coverage")
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(ENABLE_WERROR)
    add_definitions("-Werror")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions("-Weverything")
else()
    add_definitions("-Wall")
endif()

add_definitions("-Wno-sign-compare") # Needed for GTest on Ubuntu

##
##  GNU standard installation directories
##

include (GNUInstallDirs)

##
## Gettext
##

set (GETTEXT_PACKAGE "ayatana-indicator-datetime")
add_definitions (-DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
                 -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

##
##  Check for prerequisites
##

find_package (PkgConfig REQUIRED)
include (CheckIncludeFile)
include (FindPkgConfig)

set (
    SERVICE_DEPS
    libayatana-common>=0.9.3
    glib-2.0>=2.36
    gio-unix-2.0>=2.36
    libical>=0.48
    libecal-2.0>=3.16
    libedataserver-1.2>=3.5
    gstreamer-1.0>=1.2
    libnotify>=0.7.6
    properties-cpp>=0.0.1
    libaccounts-glib>=1.18
    messaging-menu>=0.8.2
    uuid>=2.25
)

if (ENABLE_LOMIRI_FEATURES)
    list (
        APPEND
        SERVICE_DEPS
        lomiri-url-dispatcher>=0
        lomiri-sounds
        lomiri-schemas
    )

    pkg_get_variable(ALARM_DEFAULT_SOUND lomiri-sounds alarm_default_sound)
    pkg_get_variable(CALENDAR_DEFAULT_SOUND lomiri-sounds calendar_default_sound)
    string(JOIN " " ALARM_DEFAULT_SOUND ${ALARM_DEFAULT_SOUND})
    string(JOIN " " CALENDAR_DEFAULT_SOUND ${CALENDAR_DEFAULT_SOUND})

    add_definitions (
        -DLOMIRI_FEATURES_ENABLED
        -DALARM_DEFAULT_SOUND="${ALARM_DEFAULT_SOUND}"
        -DCALENDAR_DEFAULT_SOUND="${CALENDAR_DEFAULT_SOUND}"
    )
else ()
    add_definitions (
        -DALARM_DEFAULT_SOUND="dummy"
        -DCALENDAR_DEFAULT_SOUND="dummy"
    )
endif ()

pkg_check_modules (SERVICE_DEPS REQUIRED ${SERVICE_DEPS})
include_directories (SYSTEM ${SERVICE_DEPS_INCLUDE_DIRS})

##
##  custom targets
##

set (ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
add_custom_target (dist
                   COMMAND bzr export --root=${ARCHIVE_NAME} ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target (cppcheck COMMAND cppcheck --enable=all -q --error-exitcode=2 --inline-suppr
                   ${CMAKE_SOURCE_DIR}/src
                   ${CMAKE_SOURCE_DIR}/tests)

##
##  Actual building
##

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)

# actually build things
add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)

# testing & coverage
if (ENABLE_TESTS)

    include(CTest)
    pkg_check_modules (DBUSTEST REQUIRED dbustest-1>=14.04.0)
    enable_testing()
    add_subdirectory(tests)

    if (ENABLE_COVERAGE)

        find_package(CoverageReport)
        ENABLE_COVERAGE_REPORT(
        TARGETS indicatordatetimeservice ayatana-indicator-datetime-service
        TESTS ${COVERAGE_TEST_TARGETS}
        FILTER /usr/include ${CMAKE_BINARY_DIR}/*
        )

    endif()

endif()

# Display config info

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Unit tests: ${ENABLE_TESTS}")
message(STATUS "Build with -Werror: ${ENABLE_WERROR}")
message(STATUS "Build with Lomiri features: ${ENABLE_LOMIRI_FEATURES}")
