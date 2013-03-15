add_definitions(-DTIMEZONE_FILE=\"/etc/timezone\")


add_library(datetime SHARED
    utils.c 
    indicator-datetime.c
)

target_link_libraries(datetime ${INDICATOR_LDFLAGS})
