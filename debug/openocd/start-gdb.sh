#!/bin/sh

# be more verbose
set -x
# exit on wrong command and undefined variables
set -e -u

# find out own directory
SCRIPTPATH=$(readlink -- "$0" || echo "$0")
SCRIPTDIR=$(CDPATH= cd -- "$(dirname -- "$SCRIPTPATH")" && pwd)
PREFIX="${SCRIPTDIR}/../../rtems/5"
export PATH="${PREFIX}/bin:${PATH}"

arm-rtems5-gdb -x ${SCRIPTDIR}/start.gdb "$@"
