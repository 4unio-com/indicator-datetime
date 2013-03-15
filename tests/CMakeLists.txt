add_definitions(-DSCHEMA_DIR=\"${CMAKE_BINARY_DIR}/tests/\")

add_library(gtest STATIC 
${GTEST_SOURCE_DIR}/gtest-all.cc 
${GTEST_SOURCE_DIR}/gtest_main.cc 
)

set_target_properties(gtest PROPERTIES INCLUDE_DIRECTORIES ${GTEST_INCLUDE_DIR})

add_custom_command(OUTPUT gschemas.compiled
COMMAND ${SCHEMA_BUILDER} ${CMAKE_SOURCE_DIR}/data --targetdir=${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(schema-compilation DEPENDS gschemas.compiled)

find_library(PTHREAD_LIB pthread)
set(TEST_LIBS gtest ${PTHREAD_LIB})

add_executable(test-indicator test-indicator.cc)
target_link_libraries(test-indicator ${INDICATOR_LDFLAGS} ${TEST_LIBS})

add_dependencies(test-indicator schema-compilation)
add_test(indicator test-indicator)
