project(indicator-datetime C)

cmake_minimum_required(VERSION 2.8.9)
option(full_warnings "Full compiler warnings." ON)
include(FindPkgConfig)

if(${full_warnings})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra")
endif()

pkg_check_modules(INDICATOR REQUIRED
indicator3-0.4>=0.3.19
glib-2.0>=2.35.4
dbusmenu-glib-0.4>=0.1.1
dbusmenu-gtk3-0.4>=0.5.90
libido3-0.1>=0.2.2
)

add_definitions(-DGETTEXT_PACKAGE=\"indicator-datetime\")
add_definitions(-DPACKAGE_NAME=\"indicator-datetime\")

include_directories(${INDICATOR_INCLUDE_DIRS})

add_subdirectory(src)
