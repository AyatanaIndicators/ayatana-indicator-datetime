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
                    DEPENDS ${CMAKE_SOURCE_DIR}/data/com.canonical.indicator.session.gschema.xml
                    COMMAND cp -f ${CMAKE_SOURCE_DIR}/data/*gschema.xml ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})

# look for hearder in our src dir, and also in the directories where we autogenerate files...
include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${CMAKE_CURRENT_BINARY_DIR} ${SERVICE_INCLUDE_DIRS})


