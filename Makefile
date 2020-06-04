# Note: $(PWD) doesn't work together with -C option of make.

.PHONY: all clean test

MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

ARCH = arm
BSP = imx7
TARGET = $(ARCH)-rtems5
PREFIX = $(MAKEFILE_DIR)/rtems/5
RSB = $(MAKEFILE_DIR)/external/rtems-source-builder
SRC_LIBBSD = $(MAKEFILE_DIR)/external/rtems-libbsd
SRC_RTEMS = $(MAKEFILE_DIR)/external/rtems
SRC_LIBGRISP = $(MAKEFILE_DIR)/external/libgrisp
SRC_LIBINIH = $(MAKEFILE_DIR)/external/libinih
SRC_BAREBOX = $(MAKEFILE_DIR)/external/barebox
BUILD_BSP = $(MAKEFILE_DIR)/build/b-$(BSP)
LIBBSD_BUILDSET = $(MAKEFILE_DIR)/src/libbsd.ini

GRISP_TOOLCHAIN_REVISION = $(shell git rev-parse HEAD)
GRISP_TOOLCHAIN_PLATFORM = grisp2
define GRISP_BUILDINFO_C
#define GRISP_TOOLCHAIN_REVISION "$(GRISP_TOOLCHAIN_REVISION)"
#define GRISP_TOOLCHAIN_PLATFORM "$(GRISP_TOOLCHAIN_PLATFORM)"
endef
export GRISP_BUILDINFO_C
define GRISP_BUILDINFO_ERL
-define(GRISP_TOOLCHAIN_REVISION, "$(GRISP_TOOLCHAIN_REVISION)").
-define(GRISP_TOOLCHAIN_PLATFORM, "$(GRISP_TOOLCHAIN_PLATFORM)").
endef
export BUILDINFO_ERL

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

# Note: You have to rebuild BSP and all libs if you change that.
DEBUG = 1
ifeq ($(DEBUG),1)
OPTIMIZATION = 0
EXTRA_BSP_OPTS = --enable-rtems-debug
else
OPTIMIZATION = 2
EXTRA_BSP_OPTS =
endif


export PATH := $(PREFIX)/bin:$(PATH)
export CFLAGS_OPTIMIZE_V ?= -O$(OPTIMIZATION) -g -ffunction-sections -fdata-sections

.PHONY: help
#H Show this help.
help:
	@grep -v grep $(MAKEFILE_LIST) | grep -A1 -h "#H" | sed -e '1!G;h;$$!d' -e 's/:[^\n]*\n/:\n\t/g' -e 's/#H//g' | grep -v -- --

.PHONY: install
#H Build and install the complete toolchain, libraries, fdt and so on.
install: submodule-update toolchain toolchain-revision bootstrap bsp libbsd fdt bsp.mk libgrisp libinih

.PHONY: submodule-update
#H Update the submodules.
submodule-update:
	git submodule update --init
	cd $(SRC_LIBBSD) && git submodule update --init rtems_waf

.PHONY: bootstrap
#H Run bootstrap for RTEMS.
bootstrap:
	cd $(SRC_RTEMS) && $(RSB)/source-builder/sb-bootstrap

.PHONY: toolchain
#H Build and install the toolchain.
toolchain:
	rm -rf $(RSB)/rtems/build
	cd $(RSB)/rtems && ../source-builder/sb-set-builder --prefix=$(PREFIX) 5/rtems-$(ARCH)
	rm -rf $(RSB)/rtems/build

.PHONY: toolchain-revision
#H Create toolchain revision files
toolchain-revision:
	mkdir -p ${PREFIX}
	echo "${GRISP_TOOLCHAIN_REVISION}" > "${PREFIX}/GRISP_TOOLCHAIN_REVISION"
	echo "${GRISP_TOOLCHAIN_PLATFORM}" > "${PREFIX}/GRISP_TOOLCHAIN_PLATFORM"
	mkdir -p ${PREFIX}/${TARGET}/${BSP}/lib/include/grisp
	echo "" > "${PREFIX}/${TARGET}/${BSP}/lib/include/grisp/grisp-buildinfo.h"
	echo "$$GRISP_BUILDINFO_C" > \
		"${PREFIX}/${TARGET}/${BSP}/lib/include/grisp/grisp-buildinfo.h"
	echo "$$GRISP_BUILDINFO_ERL" > \
		"${PREFIX}/grisp_buildinfo.hrl"

.PHONY: bsp
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
	    --disable-networking \
	    $(EXTRA_BSP_OPTS) \
	    IMX_CCM_IPG_HZ=66000000 \
	    IMX_CCM_UART_HZ=80000000 \
	    IMX_CCM_AHB_HZ=66000000 \
	    IMX_CCM_SDHCI_HZ=198000000 \
	    IMX_CCM_ECSPI_HZ=60000000
	cd $(BUILD_BSP) && make -j $(NUMCORE)
	cd $(BUILD_BSP) && make -j $(NUMCORE) install

.PHONY: bsp.mk
#H Build a Makefile helper for the applications.
bsp.mk: $(PREFIX)/make/custom/$(BSP).mk
$(PREFIX)/make/custom/$(BSP).mk: src/bsp.mk
	cat $^ | sed \
	    -e "s/##RTEMS_API##/5/g" \
	    -e "s/##RTEMS_BSP##/$(BSP)/g" \
	    -e "s/##RTEMS_CPU##/$(ARCH)/g" \
	    > $@

.PHONY: libbsd
#H Build and install libbsd.
libbsd:
	rm -rf $(SRC_LIBBSD)/build
	cd $(SRC_LIBBSD) && ./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(ARCH)/$(BSP) \
	    --enable-warnings \
	    --optimization=$(OPTIMIZATION) \
	    --buildset=$(LIBBSD_BUILDSET)
	cd $(SRC_LIBBSD) && ./waf
	cd $(SRC_LIBBSD) && ./waf install

.PHONY: libgrisp
#H Build and install libgrisp.
libgrisp:
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP) -C $(SRC_LIBGRISP) install

.PHONY: libinih
#H Build and install libinih
libinih:
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP) -C $(SRC_LIBINIH) clean install

.PHONY: fdt
#H Build the flattened device tree.
fdt:
	make PREFIX=$(PREFIX) CPP=$(TARGET)-cpp -C fdt clean all

.PHONY: barebox
#H Build the bootloader
barebox:
ifneq ($(UNAME),Linux)
	$(error Barebox can only be built on Linux)
endif
	cd $(SRC_BAREBOX) && rm -f .config
	cd $(SRC_BAREBOX) && ln -s $(MAKEFILE_DIR)/barebox/config .config
	cd $(SRC_BAREBOX) && make ARCH=$(ARCH) CROSS_COMPILE=$(TARGET)- -j$(NUMCORE)

.PHONY: demo
#H Build the demo application.
demo:
	make -C demo

.PHONY: demo-clean
#H Clean the demo application.
demo-clean:
	make -C demo clean

.PHONY: shell
#H Start a shell with the environment for building for example the RTEMS BSP.
shell:
	$(SHELL)
