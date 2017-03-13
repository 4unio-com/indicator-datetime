find_package(Intltool REQUIRED)

# NOTE: Need to add T_ to keywords, provided/used by utils.c
intltool_update_potfile(
    KEYWORDS "_" "_:1,2" "N_" "N_:1,2" "T_"
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
    COPYRIGHT_HOLDER "Canonical Ltd."
    FILE_GLOBS "${CMAKE_SOURCE_DIR}/*.gschema.xml.in.in"
    FILTER "test/*"
)

intltool_install_translations(
    ALL
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
)
