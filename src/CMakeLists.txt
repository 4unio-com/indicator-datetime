# These settings are probably incorrect. These should
# really be in a config.h file.
add_definitions(-DTIMEZONE_FILE=\"/etc/timezone\")
add_definitions(-DPKGDATADIR=\"/usr/share/indicator-datetime\")
add_definitions(-DGNOMELOCALEDIR=\"/usr/share/locale\")

add_library(datetime SHARED
    utils.c 
    indicator-datetime.c
)
target_link_libraries(datetime ${INDICATOR_LDFLAGS})


add_executable(indicator-datetime-service
    datetime-interface.c
    gen-datetime-service.xml.c
    datetime-service.c
    utils.c
)
target_link_libraries(indicator-datetime-service ${SERVICE_LDFLAGS})

include_directories(${CCPANEL_INCLUDE_DIRS})
add_library(indicator-datetime SHARED
    datetime-prefs.c
    datetime-prefs-locations.c
    utils.c
)
target_link_libraries(indicator-datetime ${CCPANEL_LDFLAGS})

install(TARGETS indicator-datetime-service
  ARCHIVE DESTINATION ${LIBDIR}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION ${LIBDIR}
)

install(TARGETS datetime
  ARCHIVE DESTINATION ${LIBDIR}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION ${LIBDIR}
)

install(TARGETS indicator-datetime
 LIBRARY DESTINATION ${CCPANELDIR}
)
