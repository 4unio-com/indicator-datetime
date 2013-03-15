add_definitions(-DTIMEZONE_FILE=\"/etc/timezone\")


add_library(datetime SHARED
    utils.c 
    indicator-datetime.c
)

add_executable(indicator-datetime-service
    datetime-interface.c
    gen-datetime-service.xml.c
    datetime-service.c
    utils.c
)

target_link_libraries(datetime ${INDICATOR_LDFLAGS})
