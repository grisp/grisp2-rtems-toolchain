#!/bin/sh

set -e
set -u
set -x

revision=076981e2f3f27306bef1c0ea5b0b458971d92236

if [ ! -e ${revision}.tar.gz ]
then
	curl -LO https://github.com/freebsd/freebsd/archive/${revision}.tar.gz
fi
tar xf ${revision}.tar.gz

if [ -e sys ]
then
	rm -r sys
fi

mkdir -p sys/gnu/dts/arm
mkdir -p sys/tools
cp -r freebsd-${revision}/sys/gnu/dts/include sys/gnu/dts
cp -r freebsd-${revision}/sys/gnu/dts/arm/imx6* sys/gnu/dts/arm
cp -r freebsd-${revision}/sys/dts sys
cp -r freebsd-${revision}/sys/tools/fdt sys/tools
rm -r freebsd-${revision}
