# Note: $(PWD) doesn't work together with -C option of make.
MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

ARCH = arm
BSP = imx7
PREFIX = $(MAKEFILE_DIR)/rtems/5
RSB = $(MAKEFILE_DIR)/external/rtems-source-builder
SRC_LIBBSD = $(MAKEFILE_DIR)/external/rtems-libbsd
SRC_RTEMS = $(MAKEFILE_DIR)/external/rtems
BUILD_BSP = $(MAKEFILE_DIR)/build/b-$(BSP)

.PHONY: fdt

export PATH := $(PREFIX)/bin:$(PATH)

help:
	echo "Use 'make all' to build complete toolchain and libraries."

all: submodule-update toolchain bootstrap bsp libbsd fdt bsp.mk

submodule-update:
	git submodule update --init
	cd $(SRC_LIBBSD) && git submodule update --init rtems_waf

bootstrap:
	cd $(SRC_RTEMS) && $(RSB)/source-builder/sb-bootstrap

toolchain:
	rm -rf $(RSB)/rtems/build
	cd $(RSB)/rtems && ../source-builder/sb-set-builder --prefix=$(PREFIX) 5/rtems-$(ARCH)
	rm -rf $(RSB)/rtems/build

bsp:
	rm -rf $(BUILD_BSP)
	mkdir -p $(BUILD_BSP)
	cd $(BUILD_BSP) && $(SRC_RTEMS)/configure \
	    --target=$(ARCH)-rtems5 \
	    --prefix=$(PREFIX) \
	    --enable-posix \
	    --enable-rtemsbsp=$(BSP) \
	    --enable-maintainer-mode \
	    --disable-rtems-debug \
	    --disable-networking
	cd $(BUILD_BSP) && make -j `nproc`
	cd $(BUILD_BSP) && make -j `nproc` install

bsp.mk: $(PREFIX)/make/custom/$(BSP).mk
$(PREFIX)/make/custom/$(BSP).mk: install/bsp.mk
	cat $^ | sed \
	    -e "s/##RTEMS_API##/$RTEMS_VERSION/g" \
	    -e "s/##RTEMS_BSP##/$BSP_NAME/g" \
	    -e "s/##RTEMS_CPU##/$RTEMS_CPU/g" \
	    > $@

libbsd:
	rm -rf $(SRC_LIBBSD)/build
	cd $(SRC_LIBBSD) && ./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(ARCH)/$(BSP) \
	    --enable-warnings \
	    --optimization=2
	cd $(SRC_LIBBSD) && ./waf
	cd $(SRC_LIBBSD) && ./waf install

fdt:
	make PREFIX=$(PREFIX) -C fdt clean all
