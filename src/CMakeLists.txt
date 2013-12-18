set (SERVICE_LIB "indicatordatetimeservice")
set (SERVICE_EXEC "indicator-datetime-service")

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ${CXX_WARNING_ARGS}")

add_definitions (-DTIMEZONE_FILE="/etc/timezone"
                 -DG_LOG_DOMAIN="Indicator-Datetime")

# let service know how to launch gnome-control-center on the desktop
if (BUILD_PANEL)
  add_definitions (-DHAVE_CCPANEL)
endif ()

add_library (${SERVICE_LIB} STATIC
             clock.cpp
             clock-live.cpp
             formatter.cpp
             formatter-desktop.cpp
             locations.cpp
             locations-settings.cpp
             planner-eds.cpp
             timezone-file.cpp
             timezone-geoclue.cpp
             timezones-live.cpp
             utils.cpp)
include_directories (${CMAKE_SOURCE_DIR})
link_directories (${SERVICE_DEPS_LIBRARY_DIRS})


#add_executable (${SERVICE_EXEC} main.c)
#target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES} ${GCOV_LIBS})
#install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

# common properties
#set_property (TARGET ${SERVICE_LIB} ${SERVICE_EXEC}
#              APPEND_STRING PROPERTY COMPILE_FLAGS
#              " -g ${CC_WARNING_ARGS} ${GCOV_FLAGS}")


