set (SERVICE_LIB "indicatordatetimeservice")
set (SERVICE_EXEC "indicator-datetime-service")

add_definitions (-DG_LOG_DOMAIN="Indicator-Datetime")

# handwritten sources
set (SERVICE_C_SOURCES
     utils.c)
set (SERVICE_CXX_SOURCES
     actions.cpp
     actions-live.cpp
     alarm-queue-simple.cpp
     awake.cpp
     appointment.cpp
     clock.cpp
     clock-live.cpp
     date-time.cpp
     engine-eds.cpp
     exporter.cpp
     formatter.cpp
     formatter-desktop.cpp
     haptic.cpp
     locations.cpp
     locations-settings.cpp
     menu.cpp
     myself.cpp
     notifications.cpp
     planner.cpp
     planner-aggregate.cpp
     planner-snooze.cpp
     planner-month.cpp
     planner-range.cpp
     planner-upcoming.cpp
     settings-live.cpp
     snap.cpp
     sound.cpp
     timezone-geoclue.cpp
     timezones-live.cpp
     timezone-timedated.cpp
     utils.c
     wakeup-timer-mainloop.cpp
     wakeup-timer-powerd.cpp)

# generated sources
include (GdbusCodegen)
set(SERVICE_GENERATED_SOURCES)
add_gdbus_codegen(SERVICE_GENERATED_SOURCES dbus-alarm-properties
                  com.canonical.indicator
                  ${CMAKE_SOURCE_DIR}/data/com.canonical.indicator.datetime.AlarmProperties.xml)
add_gdbus_codegen(SERVICE_GENERATED_SOURCES dbus-accounts-sound
                  com.ubuntu.touch
                  ${CMAKE_SOURCE_DIR}/src/com.ubuntu.touch.AccountsService.Sound.xml)

# add warnings/coverage info on handwritten files
# but not the autogenerated ones...
set_source_files_properties(${SERVICE_CXX_SOURCES}
                            PROPERTIES COMPILE_FLAGS "${CXX_WARNING_ARGS} -std=c++11")
set_source_files_properties(${SERVICE_C_SOURCES}
                            PROPERTIES COMPILE_FLAGS "${CXX_WARNING_ARGS} -std=c99")

# add the bin dir to our include path so our code can find the generated header files
include_directories (${CMAKE_CURRENT_BINARY_DIR})

add_library (${SERVICE_LIB} STATIC ${SERVICE_C_SOURCES} ${SERVICE_CXX_SOURCES} ${SERVICE_GENERATED_SOURCES})
include_directories (${CMAKE_SOURCE_DIR})
link_directories (${SERVICE_DEPS_LIBRARY_DIRS})

add_executable (${SERVICE_EXEC} main.cpp)
set_source_files_properties(${SERVICE_SOURCES} main.cpp PROPERTIES COMPILE_FLAGS "${CXX_WARNING_ARGS} -std=c++11")
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

