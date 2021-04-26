project (ayatana-indicator-datetime C CXX)
cmake_minimum_required (VERSION 2.8.9)

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include (GNUInstallDirs)

set (PROJECT_VERSION "0.8.3")
set (PACKAGE ${CMAKE_PROJECT_NAME})
option (enable_tests "Build the package's automatic tests." ON)
option (enable_lcov "Generate lcov code coverage reports." ON)

##
##  GNU standard installation directories
##

set (CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set (CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")

##
## Gettext
##

set (GETTEXT_PACKAGE "ayatana-indicator-datetime")
add_definitions (-DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
                 -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

set(ALARM_DEFAULT_SOUND    "/usr/share/sounds/ubuntu/ringtones/Alarm clock.ogg")
set(CALENDAR_DEFAULT_SOUND "/usr/share/sounds/ubuntu/ringtones/Marimbach.ogg")
add_definitions(-DALARM_DEFAULT_SOUND="${ALARM_DEFAULT_SOUND}"
                -DCALENDAR_DEFAULT_SOUND="${CALENDAR_DEFAULT_SOUND}")

##
##  Check for prerequisites
##

find_package (PkgConfig REQUIRED)
include (CheckIncludeFile)
include (FindPkgConfig)

pkg_check_modules (SERVICE_DEPS REQUIRED
                   glib-2.0>=2.36
                   gio-unix-2.0>=2.36
                   libical>=0.48
                   libecal-2.0>=3.16
                   libedataserver-1.2>=3.5
                   gstreamer-1.0>=1.2
                   libnotify>=0.7.6
                   properties-cpp>=0.0.1
                   libaccounts-glib>=1.18)
include_directories (SYSTEM ${SERVICE_DEPS_INCLUDE_DIRS})

set(URL_DISPATCHER_REQUIRED_VERSION 0)
pkg_check_modules(URLDISPATCHER
                  lomiri-url-dispatcher>=${URL_DISPATCHER_REQUIRED_VERSION}
)
include_directories(${URLDISPATCHER_INCLUDE_DIRS})

# url-dispatcher support is optional...
if(URLDISPATCHER_FOUND)
    add_definitions( -DHAS_URLDISPATCHER )
endif()

if(EXISTS "/usr/share/accountsservice/interfaces/com.ubuntu.touch.AccountsService.Sound.xml")
  set (HAVE_UT_ACCTSERVICE_SYSTEMSOUND_SETTINGS ON)
  add_definitions ( -DHAS_UT_ACCTSERVICE_SYSTEMSOUND_SETTINGS )
else()
  set (HAVE_UT_ACCTSERVICE_SYSTEMSOUND_SETTINGS OFF)
endif()

##
##  custom targets
##

set (ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
add_custom_target (dist
                   COMMAND bzr export --root=${ARCHIVE_NAME} ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target (clean-coverage
                   COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' | xargs rm -f)

add_custom_target (cppcheck COMMAND cppcheck --enable=all -q --error-exitcode=2 --inline-suppr
                   ${CMAKE_SOURCE_DIR}/src
                   ${CMAKE_SOURCE_DIR}/tests)

##
##  Actual building
##

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    set(COMPILE_FLAGS "${COMPILE_FLAGS} -Weverything")
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-c++98-compat -Wno-padded")
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-documentation")

else()

    set(COMPILE_FLAGS "${COMPILE_FLAGS} -Wall")

endif()


include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)

# testing & coverage
if (${enable_tests})
  pkg_check_modules (DBUSTEST REQUIRED dbustest-1>=14.04.0)

  if (EXISTS /usr/src/googletest/src)
    set (GTEST_SOURCE_DIR /usr/src/googletest/src)
  else ()
    set (GTEST_SOURCE_DIR /usr/src/gtest/src)
  endif ()

  set (GTEST_INCLUDE_DIR ${GTEST_SOURCE_DIR}/..)
  set (GTEST_LIBS -lpthread)
  enable_testing ()
  if (${enable_lcov})
    include(GCov)
  endif ()
endif ()

# actually build things
add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)
if (${enable_tests})
  add_subdirectory(tests)
endif ()
