find_package(Intltool REQUIRED)

intltool_update_potfile(
    KEYWORDS "_" "_:1,2" "N_" "N_:1,2"
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
    COPYRIGHT_HOLDER "Canonical Ltd."
)

intltool_install_translations(
    ALL
    GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
)
