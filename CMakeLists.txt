project(indicator-datetime C CXX)

cmake_minimum_required(VERSION 2.8.9)
option(full_warnings "Full compiler warnings." ON)
include(FindPkgConfig)

if(${full_warnings})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")
endif()

enable_testing()

find_program(SCHEMA_BUILDER "glib-compile-schemas")

pkg_check_modules(INDICATOR REQUIRED
indicator3-0.4>=0.3.19
glib-2.0>=2.35.4
dbusmenu-glib-0.4>=0.1.1
dbusmenu-gtk3-0.4>=0.5.90
libido3-0.1>=0.2.2
)

pkg_check_modules(SERVICE REQUIRED
indicator3-0.4>=0.3.19
glib-2.0>=2.35.4
dbusmenu-glib-0.4>=0.1.1
dbusmenu-gtk3-0.4>=0.5.90
libido3-0.1>=0.2.2
gio-2.0>=2.25.11
geoclue>=0.12.0
libecal-1.2>=3.5
libical>=0.48
libedataserver-1.2>=3.5
libedataserverui-3.0>=3.5
cairo>=1.10
gdk-3.0>=2.22
)

include_directories(${SERVICE_INCLUDE_DIRS})

add_definitions(-DGETTEXT_PACKAGE=\"indicator-datetime\")
add_definitions(-DPACKAGE_NAME=\"indicator-datetime\")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)
add_subdirectory(tests)
