#!/bin/sh

echo ARG0=$0 # this script
echo ARG1=$1 # full executable path of dbus-test-runner
echo ARG2=$2 # full executable path of test app
echo ARG3=$3 # test name
echo ARG4=$4 # full executable path of evolution-calendar-factory
echo ARG5=$5 # bus service name of calendar factory
echo ARG6=$6 # full exectuable path of evolution-source-registry
echo ARG7=$7 # full executable path of gvfs
echo ARG8=$8 # config files

export TEST_TMP_DIR=$(mktemp -p "${TMPDIR:-.}" -d $3-XXXXXXXXXX) || exit 1
trap 'rm -rf $TEST_TMP_DIR' EXIT
echo "running test '$3' in ${TEST_TMP_DIR}"

export QT_QPA_PLATFORM=minimal
export HOME=${TEST_TMP_DIR}
export XDG_RUNTIME_DIR=${TEST_TMP_DIR}
export XDG_CACHE_HOME=${TEST_TMP_DIR}/.cache
export XDG_CONFIG_HOME=${TEST_TMP_DIR}/.config
export XDG_DATA_HOME=${TEST_TMP_DIR}/.local/share
export XDG_DESKTOP_DIR=${TEST_TMP_DIR}
export XDG_DOCUMENTS_DIR=${TEST_TMP_DIR}
export XDG_DOWNLOAD_DIR=${TEST_TMP_DIR}
export XDG_MUSIC_DIR=${TEST_TMP_DIR}
export XDG_PICTURES_DIR=${TEST_TMP_DIR}
export XDG_PUBLICSHARE_DIR=${TEST_TMP_DIR}
export XDG_TEMPLATES_DIR=${TEST_TMP_DIR}
export XDG_VIDEOS_DIR=${TEST_TMP_DIR}
export QORGANIZER_EDS_DEBUG=On
export GIO_USE_VFS=local # needed to ensure GVFS shuts down cleanly after the test is over

echo HOMEDIR=${HOME}
rm -rf ${XDG_DATA_HOME}

# if there are canned config files for this test, move them into place now
if [ -d $8 ]; then
  echo "copying files from $8 to $HOME"
  cp --verbose --archive $8/. $HOME
fi

$1 --keep-env --max-wait=90 \
--task $2 --task-name $3 --wait-until-complete --wait-for=org.gnome.evolution.dataserver.Calendar4 \
--task $4 --task-name "evolution" --wait-until-complete -r
#--task $6 --task-name "source-registry" --wait-for=org.gtk.vfs.Daemon -r \
#--task $7 --task-name "gvfsd" -r
return $?
