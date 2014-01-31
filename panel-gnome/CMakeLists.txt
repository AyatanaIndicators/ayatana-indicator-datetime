set (PANEL_LIB "gnome-indicator-datetime")

add_definitions (-DPKGDATADIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}")


SET (CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -g")
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ${CXX_WARNING_ARGS}")

add_library (${PANEL_LIB} SHARED
             ${CMAKE_SOURCE_DIR}/panel/datetime-prefs.c 
             ${CMAKE_SOURCE_DIR}/panel/datetime-prefs-locations.c
             ${CMAKE_SOURCE_DIR}/panel/datetime-prefs-locations.h
             ${CMAKE_SOURCE_DIR}/src/utils.c
             ${CMAKE_SOURCE_DIR}/include/datetime/utils.h
             ${CMAKE_SOURCE_DIR}/include/datetime/settings-shared.h)
set_property (TARGET ${PANEL_LIB} PROPERTY OUTPUT_NAME indicator-datetime)

include_directories (SYSTEM ${PANEL_DEPS_INCLUDE_DIRS})

link_directories (${PANEL_DEPS_LIBRARY_DIRS})

set_property (TARGET ${PANEL_LIB}
              APPEND_STRING PROPERTY COMPILE_FLAGS
              " -g ${CC_WARNING_ARGS} ${GCOV_FLAGS}")

target_link_libraries (${PANEL_LIB} ${PANEL_DEPS_LIBRARIES} ${GCOV_LIBS})

install (TARGETS ${PANEL_LIB}
         DESTINATION ${CMAKE_INSTALL_LIBDIR}/control-center-1/panels)

