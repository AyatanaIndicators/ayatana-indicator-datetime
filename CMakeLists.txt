project (indicator-datetime C CXX)
cmake_minimum_required (VERSION 2.8.9)

list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set (PROJECT_VERSION "14.04.0")
set (PACKAGE ${CMAKE_PROJECT_NAME})
set (GETTEXT_PACKAGE "indicator-datetime")
add_definitions (-DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
                 -DGNOMELOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

option (enable_tests "Build the package's automatic tests." ON)
option (enable_lcov "Generate lcov code coverage reports." ON)

##
##  GNU standard installation directories
## 

include (GNUInstallDirs)
if (EXISTS "/etc/debian_version") # Workaround for libexecdir on debian
  set (CMAKE_INSTALL_LIBEXECDIR "${CMAKE_INSTALL_LIBDIR}")
  set (CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif ()
set (CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set (CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")

##
##  Check for prerequisites
##

find_package (PkgConfig REQUIRED)
include (FindPkgConfig)

pkg_check_modules (SERVICE_DEPS REQUIRED
                   glib-2.0>=2.36
                   gio-unix-2.0>=2.36
                   geoclue>=0.12
                   libical>=0.48
                   libecal-1.2>=3.5
                   libedataserver-1.2>=3.5
                   libnotify>=0.7.6
                   url-dispatcher-1>=1
                   properties-cpp>=0.0.1)
include_directories (SYSTEM ${SERVICE_DEPS_INCLUDE_DIRS})

pkg_check_modules (PANEL_DEPS
                   glib-2.0>=2.36
                   gio-unix-2.0>=2.36
                   gtk+-3.0>=3.1.4
                   timezonemap
                   libgnome-control-center
                   polkit-gobject-1)
if (PANEL_DEPS_FOUND)
  set (BUILD_PANEL 1)
endif ()

pkg_check_modules (UNITY_PANEL_DEPS
                   glib-2.0>=2.36
                   gio-unix-2.0>=2.36
                   gtk+-3.0>=3.1.4
                   timezonemap
                   libunity-control-center
                   polkit-gobject-1)
if (UNITY_PANEL_DEPS_FOUND)
  set (BUILD_UNITY_PANEL 1)
endif ()

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

set (CC_WARNING_ARGS " -Wall -Wshadow -Wextra -Wunused -Wformat=2 -Wno-missing-field-initializers")
set (CXX_WARNING_ARGS " -Wall -Wextra -pedantic -Wno-missing-field-initializers")

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)

# testing & coverage
if (${enable_tests})
  pkg_check_modules (DBUSTEST REQUIRED dbustest-1>=14.04.0)
  set (GTEST_SOURCE_DIR /usr/src/gtest/src)
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
if (BUILD_PANEL)
  add_subdirectory (panel-gnome)
endif ()
if (BUILD_UNITY_PANEL)
  add_subdirectory (panel-unity)
endif ()
add_subdirectory(data)
add_subdirectory(po)
if (${enable_tests})
  add_subdirectory(tests)
endif ()

