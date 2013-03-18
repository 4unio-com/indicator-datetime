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

set(GTEST_SOURCE_DIR /usr/src/gtest/src)
set(GTEST_INCLUDE_DIR ${GTEST_SOURCE_DIR}/..)

set(INDICATOR_REQUIRED_VERSION 0.3.19)
set(DBUSMENUGLIB_REQUIRED_VERSION 0.1.1)
set(DBUSMENUGTK_REQUIRED_VERSION 0.5.90)
set(GIO_REQUIRED_VERSION 2.25.11)
set(INDICATOR_DISPLAY_OBJECTS 0.2.2)
set(GEOCLUE_REQUIRED_VERSION 0.12.0)
set(ECAL_REQUIRED_VERSION 3.5)
set(EDS_REQUIRED_VERSION 3.5)
set(ICAL_REQUIRED_VERSION 0.48)
set(CAIRO_REQUIRED_VERSION 1.10)
set(GDK_REQUIRED_VERSION 2.22)
set(GLIB_REQUIRED_VERSION 2.35.4)
set(GTK3_REQUIRED_VERSION 3.1.4)

pkg_check_modules(INDICATOR REQUIRED
indicator3-0.4>=${INDICATOR_REQUIRED_VERSION}
glib-2.0>=${GLIB_REQUIRED_VERSION}
dbusmenu-glib-0.4>=${DBUSMENUGLIB_REQUIRED_VERSION}
dbusmenu-gtk3-0.4>=${DBUSMENUGTK_REQUIRED_VERSION}
libido3-0.1>=${INDICATOR_DISPLAY_OBJECTS}
)

pkg_check_modules(SERVICE REQUIRED
indicator3-0.4>=${INDICATOR_REQUIRED_VERSION}
glib-2.0>=${GLIB_REQUIRED_VERSION}
dbusmenu-glib-0.4>=${DBUSMENUGLIB_REQUIRED_VERSION}
dbusmenu-gtk3-0.4>=${DBUSMENUGTK_REQUIRED_VERSION}
libido3-0.1>=${INDICATOR_DISPLAY_OBJECTS}
gio-2.0>=${GIO_REQUIRED_VERSION}
geoclue>=${GEOCLUE_REQUIRED_VERSION}
libecal-1.2>=${ECAL_REQUIRED_VERSION}
libical>=${ICAL_REQUIRED_VERSION}
libedataserver-1.2>=${EDS_REQUIRED_VERSION}
libedataserverui-3.0>=${EDS_REQUIRED_VERSION}
cairo>=${CAIRO_REQUIRED_VERSION}
gdk-3.0>=${GDK_REQUIRED_VERSION}
)


pkg_check_modules(CCPANEL REQUIRED
gio-2.0>=${GIO_REQUIRED_VERSION}
gtk+-3.0>=${GTK3_REQUIRED_VERSION}
timezonemap
libgnome-control-center
polkit-gobject-1
)

set(LIBDIR "lib" CACHE PATH "Destination install dir for shared libraries")
set(CCPANELDIR "lib" CACHE PATH "Destination directory for the ccpanel library")

include_directories(${SERVICE_INCLUDE_DIRS})

add_definitions(-DGETTEXT_PACKAGE=\"indicator-datetime\")
add_definitions(-DPACKAGE_NAME=\"indicator-datetime\")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(data)

