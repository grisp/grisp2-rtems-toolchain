ARCH = arm
BSP = imx7
PREFIX = $(PWD)/rtems/5
RSB = $(PWD)/external/rtems-source-builder
SRC_LIBBSD = $(PWD)/external/rtems-libbsd
SRC_RTEMS = $(PWD)/external/rtems
BUILD_BSP = $(PWD)/build/b-$(BSP)

export PATH := $(PREFIX)/bin:$(PATH)

.PHONY: libbsd

all: submodule-update toolchain bootstrap bsp libbsd

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

libbsd:
	rm -rf $(SRC_LIBBSD)/build
	cd $(SRC_LIBBSD) && ./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(ARCH)/$(BSP) \
	    --enable-warnings \
	    --optimization=2
	cd $(SRC_LIBBSD) && ./waf
	cd $(SRC_LIBBSD) && ./waf install
