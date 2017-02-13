find_package(Intltool REQUIRED)

# Must pass all flie globs we want translated here, due to cmake-extras bug
# in Intltool module, to get the schemas translation strings (LP: #1664345)
# NOTE: Need to add T_ to keywords, provided/used by utils.c
intltool_update_potfile(
    KEYWORDS "_" "_:1,2" "N_" "N_:1,2" "T_"
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
    COPYRIGHT_HOLDER "Canonical Ltd."
    FILE_GLOBS "${CMAKE_SOURCE_DIR}/*.gschema.xml.in.in" "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h" "${CMAKE_SOURCE_DIR}/*.c"
    FILTER "test/*"
)

intltool_install_translations(
    ALL
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
)
