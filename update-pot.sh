#!/bin/bash

# Copyright (C) 2017 by Mike Gabriel <mike.gabriel@das-netzwerkteam.de>
#
# This package is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3 of the License.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>

GETTEXT_DOMAIN=$(cat CMakeLists.txt | grep 'set.*(.*GETTEXT_PACKAGE' | sed -r -e 's/.*\"([^"]+)\"\)/\1/')

# Rename function calls 'T_([...])' to 'T _([...])'
# This hack is needed to also process T_() function calls correctly with intltool-update
# (in time formatter code).
cd src/ && for file in *.cpp *.c; do sed -e "s/ T_/ T _/g" -i $file; done && cd - 1>/dev/null

# Yet another hack to include .gschema.xml translations
sed -i po/POTFILES.in -e 's/org.ayatana.indicator.datetime.gschema.xml.in$/org.ayatana.indicator.datetime.gschema.xml/g'
cp data/org.ayatana.indicator.datetime.gschema.xml.in data/org.ayatana.indicator.datetime.gschema.xml

# Run the intltool-update...
cd po/ && intltool-update --gettext-package ${GETTEXT_DOMAIN} --pot && cd - 1>/dev/null

# And revert...
sed -i po/POTFILES.in -e 's/org.ayatana.indicator.datetime.gschema.xml$/org.ayatana.indicator.datetime.gschema.xml.in/g'
rm data/org.ayatana.indicator.datetime.gschema.xml

# And undo the renamings again.
cd src/ && for file in *.cpp *.c; do sed -e "s/ T _/ T_/g" -i $file; done && cd - 1>/dev/null

sed -E					\
    -e 's/\.xml\.in\.h:/.xml.in:/g'	\
    -e 's/\.ini\.in\.h:/.ini.in:/g'	\
    -e 's/\.xml\.h:/.xml:/g'		\
    -e 's/\.ini\.h:/.ini:/g'		\
    -e 's@^#: \.\./@#: @g'		\
    -e 's@(:[0-9]+) \.\./@\1 @g'	\
    -i po/${GETTEXT_DOMAIN}.pot
