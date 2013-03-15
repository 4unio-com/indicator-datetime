# FIXME, set variables so they are substituted properly.
configure_file(indicator-datetime.service.in indicator-datetime.service)

install(FILES
${CMAKE_CURRENT_BINARY_DIR}/indicator-datetime.service
DESTINATION share/indicator-datetime # FIXME to be the real install dir.
)

install(FILES
datetime-dialog.ui
DESTINATION share/indicatore-datetime # FIXME this too.
)
