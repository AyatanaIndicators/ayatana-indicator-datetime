#!/bin/bash

GETTEXT_DOMAIN=$(cat CMakeLists.txt | grep 'set.*(.*GETTEXT_PACKAGE' | sed -r -e 's/.*\"([^"]+)\"\)/\1/')

cd po/ && intltool-update --gettext-package ${GETTEXT_DOMAIN} --pot
