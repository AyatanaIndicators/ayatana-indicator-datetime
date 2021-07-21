#!/bin/sh

SELF=$0        # this script
TEST_RUNNER=$1 # full executable path of dbus-test-runner
TEST_EXEC=$2   # full executable path of test app
TEST_NAME=$3   # test name
CONFIG_DIR=$4  # config files
ICS_FILE=$5    # ical file holding test data
ACCOUNTS_DB=$6 # online account database

echo "this script: ${SELF}"
echo "test-runner: ${TEST_RUNNER}"
echo "test-exec: ${TEST_EXEC}"
echo "test-name: ${TEST_NAME}"
echo "config-dir: ${CONFIG_DIR}"
echo "ics-file: ${ICS_FILE}"

# set up the tmpdir
export TEST_TMP_DIR=$(mktemp -p "${TMPDIR:-/tmp}" -d ${TEST_NAME}-XXXXXXXXXX) || exit 1
echo "running test '${TEST_NAME}' in ${TEST_TMP_DIR}"

# set up the environment variables
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

export G_MESSAGES_DEBUG=all
export G_DBUS_DEBUG=messages

echo HOMEDIR=${HOME}
rm -rf ${XDG_DATA_HOME}

# if there are canned config files for this test, move them into place now
if [ -d ${CONFIG_DIR} ]; then
  echo "copying files from ${CONFIG_DIR} to $HOME"
  cp --verbose --archive ${CONFIG_DIR}/. $HOME
fi

# if there's a specific ics file to test, copy it on top of the canned config files
if [ -e ${ICS_FILE} ]; then
  echo "copying ${ICS_FILE} into $HOME"
  mkdir -p ${XDG_DATA_HOME}/evolution/tasks/system/
  cp --verbose --archive ${ICS_FILE} ${XDG_DATA_HOME}/evolution/tasks/system/tasks.ics
fi

# prepare online accounts database
if [ -e ${ACCOUNTS_DB} ]; then
  echo "copying ${ACCOUNTS_DB} into $HOME"
  mkdir -p ${XDG_CONFIG_HOME}/libaccounts-glib/
  cp --verbose --archive ${ACCOUNTS_DB} ${XDG_CONFIG_HOME}/libaccounts-glib/accounts.db
fi

# run the test
${TEST_RUNNER} --keep-env --max-wait=90 --task ${TEST_EXEC} --task-name ${TEST_NAME} --wait-until-complete
rv=$?

# if the test passed, blow away the tmpdir
if [ $rv -eq 0 ]; then
    rm -rf $TEST_TMP_DIR
fi
