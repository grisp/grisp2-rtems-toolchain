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

openocd -s "${PREFIX}/share/openocd/scripts/" \
	-f "${SCRIPTDIR}/openocd-grisp2.cfg" \
	-c "init" \
	-c "reset halt" \
	"$@"
