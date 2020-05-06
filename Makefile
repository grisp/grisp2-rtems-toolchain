# Note: $(PWD) doesn't work together with -C option of make.
MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

ARCH = arm
BSP = imx7
PREFIX = $(MAKEFILE_DIR)/rtems/5
RSB = $(MAKEFILE_DIR)/external/rtems-source-builder
SRC_LIBBSD = $(MAKEFILE_DIR)/external/rtems-libbsd
SRC_RTEMS = $(MAKEFILE_DIR)/external/rtems
SRC_LIBGRISP = $(MAKEFILE_DIR)/external/libgrisp
SRC_BAREBOX = $(MAKEFILE_DIR)/external/barebox
BUILD_BSP = $(MAKEFILE_DIR)/build/b-$(BSP)
LIBBSD_BUILDSET = $(MAKEFILE_DIR)/src/libbsd.ini

UNAME := $(shell uname -s)

# macOS and FreeBSD
ifneq (,$(filter $(UNAME),Darwin FreeBSD))
	NUMCORE = $(shell sysctl -n hw.ncpu)
# Linux
else ifeq ($(UNAME),Linux)
	NUMCORE = $(shell nproc)
else
	NUMCORE = 1
endif

.PHONY: fdt demo demo-clean

export PATH := $(PREFIX)/bin:$(PATH)
export CFLAGS_OPTIMIZE_V ?= -O0 -g -ffunction-sections -fdata-sections

#H Show this help.
help:
	@grep -v grep $(MAKEFILE_LIST) | grep -A1 -h "#H" | sed -e '1!G;h;$$!d' -e 's/:[^\n]*\n/:\n\t/g' -e 's/#H//g' | grep -v -- --

#H Build and install the complete toolchain, libraries, fdt and so on.
install: submodule-update toolchain bootstrap bsp libbsd fdt bsp.mk libgrisp

#H Update the submodules.
submodule-update:
	git submodule update --init
	cd $(SRC_LIBBSD) && git submodule update --init rtems_waf

#H Run bootstrap for RTEMS.
bootstrap:
	cd $(SRC_RTEMS) && $(RSB)/source-builder/sb-bootstrap

#H Build and install the toolchain.
toolchain:
	rm -rf $(RSB)/rtems/build
	cd $(RSB)/rtems && ../source-builder/sb-set-builder --prefix=$(PREFIX) 5/rtems-$(ARCH)
	rm -rf $(RSB)/rtems/build

#H Build the RTEMS board support package.
bsp:
	rm -rf $(BUILD_BSP)
	mkdir -p $(BUILD_BSP)
	cd $(BUILD_BSP) && $(SRC_RTEMS)/configure \
	    --target=$(ARCH)-rtems5 \
	    --prefix=$(PREFIX) \
	    --enable-posix \
	    --enable-rtemsbsp=$(BSP) \
	    --enable-maintainer-mode \
	    --enable-rtems-debug \
	    --disable-networking \
	    IMX_CCM_IPG_HZ=66000000 \
	    IMX_CCM_UART_HZ=80000000 \
	    IMX_CCM_AHB_HZ=66000000 \
	    IMX_CCM_SDHCI_HZ=198000000 \
	    IMX_CCM_ECSPI_HZ=60000000
	cd $(BUILD_BSP) && make -j $(NUMCORE)
	cd $(BUILD_BSP) && make -j $(NUMCORE) install

#H Build a Makefile helper for the applications.
bsp.mk: $(PREFIX)/make/custom/$(BSP).mk
$(PREFIX)/make/custom/$(BSP).mk: src/bsp.mk
	cat $^ | sed \
	    -e "s/##RTEMS_API##/5/g" \
	    -e "s/##RTEMS_BSP##/$(BSP)/g" \
	    -e "s/##RTEMS_CPU##/$(ARCH)/g" \
	    > $@

#H Build and install libbsd.
libbsd:
	rm -rf $(SRC_LIBBSD)/build
	cd $(SRC_LIBBSD) && ./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(ARCH)/$(BSP) \
	    --enable-warnings \
	    --optimization=0 \
	    --buildset=$(LIBBSD_BUILDSET)
	cd $(SRC_LIBBSD) && ./waf
	cd $(SRC_LIBBSD) && ./waf install

#H Build and install libgrisp.
libgrisp:
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP) -C $(SRC_LIBGRISP) install

#H Build the flattened device tree.
fdt:
	make PREFIX=$(PREFIX) CPP=arm-rtems5-cpp -C fdt clean all

.PHONY: barebox
#H Build the bootloader
barebox:
	ifneq ($(UNAME),Linux)
		$(error Barebox can only be built on Linux)
	else
	cd $(SRC_BAREBOX) && rm -f .config
	cd $(SRC_BAREBOX) && ln -s $(MAKEFILE_DIR)/barebox/config .config
	cd $(SRC_BAREBOX) && make ARCH=arm CROSS_COMPILE=arm-rtems5- -j$(NUMCORE)

#H Build the demo application.
demo:
	make -C demo

#H Clean the demo application.
demo-clean:
	make -C demo clean

#H Start a shell with the environment for building for example the RTEMS BSP.
shell:
	$(SHELL)
