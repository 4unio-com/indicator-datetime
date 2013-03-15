add_definitions(-DSCHEMA_DIR=\"${CMAKE_BINARY_DIR}/tests/\")

add_custom_command(OUTPUT gschemas.compiled
COMMAND ${SCHEMA_BUILDER} ${CMAKE_SOURCE_DIR}/data --targetdir=${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(schema-compilation DEPENDS gschemas.compiled)

add_executable(test-indicator test-indicator.cc)
target_link_libraries(test-indicator ${INDICATOR_LDFLAGS})

add_dependencies(test-indicator schema-compilation)
add_test(indicator test-indicator)
