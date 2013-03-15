add_definitions(-DTIMEZONE_FILE=\"/etc/timezone\")

include_directories(${SERVICE_INCLUDE_DIRS})

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
