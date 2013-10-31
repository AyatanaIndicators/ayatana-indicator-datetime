set (SERVICE_LIB "libindicatordatetimeservice")
set (SERVICE_EXEC "indicator-datetime-service")

add_definitions (-DTIMEZONE_FILE="/etc/timezone"
                 -DG_LOG_DOMAIN="Indicator-Datetime")

# let service know how to launch gnome-control-center on the desktop
if (BUILD_PANEL)
  add_definitions (-DHAVE_CCPANEL)
endif ()

add_library (${SERVICE_LIB} STATIC
             clock.c
             clock.h
             clock-live.c
             clock-live.h
             planner.c
             planner.h
             planner-eds.c
             planner-eds.h
             service.c
             service.h
             timezone.c
             timezone.h
             timezone-file.c
             timezone-file.h
             timezone-geoclue.c
             timezone-geoclue.h
             utils.c
             utils.h
             dbus-shared.h
             settings-shared.h)
include_directories (${SERVICE_DEPS_INCLUDE_DIRS})
link_directories (${SERVICE_DEPS_LIBRARY_DIRS})

add_executable (${SERVICE_EXEC} main.c)
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES} ${GCOV_LIBS})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

# common properties
set_property (TARGET ${SERVICE_LIB} ${SERVICE_EXEC}
              APPEND_STRING PROPERTY COMPILE_FLAGS
              " -g ${CC_WARNING_ARGS} ${GCOV_FLAGS}")


