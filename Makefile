# Note: $(PWD) doesn't work together with -C option of make.

.PHONY: all clean test

MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

ARCH = arm
BSP = imx7
BSP_GRISP1 = atsamv
RTEMS_VERSION = 5
TARGET = $(ARCH)-rtems$(RTEMS_VERSION)
PREFIX = $(MAKEFILE_DIR)/rtems/$(RTEMS_VERSION)
RSB = $(MAKEFILE_DIR)/external/rtems-source-builder
SRC_LIBBSD = $(MAKEFILE_DIR)/external/rtems-libbsd
SRC_RTEMS = $(MAKEFILE_DIR)/external/rtems
SRC_LIBGRISP = $(MAKEFILE_DIR)/external/libgrisp
SRC_LIBINIH = $(MAKEFILE_DIR)/external/libinih
SRC_BAREBOX = $(MAKEFILE_DIR)/external/barebox
SRC_OPENOCD = $(MAKEFILE_DIR)/external/openocd-code
SRC_IMX_USB_LOADER = $(MAKEFILE_DIR)/external/imx_usb_loader
SRC_CRYPTOAUTHLIB = $(MAKEFILE_DIR)/external/cryptoauthlib
SRC_OPENBLAS = $(MAKEFILE_DIR)/external/OpenBLAS
BUILD_BSP = $(MAKEFILE_DIR)/build/b-$(BSP)
BUILD_BSP_GRISP1 = $(MAKEFILE_DIR)/build/b-$(BSP_GRISP1)
BUILD_LOGS = $(MAKEFILE_DIR)/build
BUILD_OPENOCD = $(MAKEFILE_DIR)/build/b-openocd
LIBBSD_BUILDSET = $(MAKEFILE_DIR)/src/libbsd.ini
CMAKE_TOOLCHAIN_TEMPLATE = $(MAKEFILE_DIR)/cryptoauthlib/grisp2-toolchain.cmake.in
CMAKE_TOOLCHAIN_CONFIG = $(PREFIX)/share/grisp2-toolchain.cmake
PACKAGE_DIR = package
PACKAGE_PREFIX = grisp2-rtems-toolchain

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
	OS_NAME = macOS
	OS_VERSION = $(shell sw_vers -productVersion)
# Linux
else ifeq ($(UNAME),Linux)
	NUMCORE = $(shell nproc)
	OS_NAME = Linux
	OS_VERSION = $(shell uname -r)
else
	NUMCORE = 1
	OS_NAME = unknown
	OS_VERSION = unknown
endif

# Note: You have to rebuild BSP and all libs if you change that.
DEBUG = 0
ifeq ($(DEBUG),1)
OPTIMIZATION = 0
EXTRA_BSP_OPTS = --enable-rtems-debug
else
OPTIMIZATION = 2
EXTRA_BSP_OPTS =
endif


export ORGPATH := $(PATH)
export PATH := $(PREFIX)/bin:$(PATH)
export CFLAGS_OPTIMIZE_V ?= -O$(OPTIMIZATION) -g -ffunction-sections -fdata-sections

.PHONY: help
#H Show this help.
help:
	@grep -v grep $(MAKEFILE_LIST) | grep -A1 -h "#H" | sed -e '1!G;h;$$!d' -e 's/:[^\n]*\n/:\n\t/g' -e 's/#H//g' | grep -v -- --

.PHONY: install
#H Build and install the complete toolchain, libraries, fdt and so on.
install: submodule-update toolchain toolchain-revision bootstrap bsp bsp-grisp1 libbsd fdt bsp.mk libgrisp libinih cryptoauthlib barebox-install #OpenBLAS

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
	mkdir -p $(BUILD_LOGS)
	rm -rf $(RSB)/rtems/build
	cd $(RSB)/rtems && ../source-builder/sb-set-builder \
	    --prefix=$(PREFIX) \
	    --log=$(BUILD_LOGS)/rsb-toolchain.log \
	    $(RTEMS_VERSION)/rtems-$(ARCH)
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
	    --target=$(ARCH)-rtems$(RTEMS_VERSION) \
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

.PHONY: bsp-grisp1
#H Build the RTEMS board support package for GRiSP1.
bsp-grisp1:
	rm -rf $(BUILD_BSP_GRISP1)
	mkdir -p $(BUILD_BSP_GRISP1)
	cd $(BUILD_BSP_GRISP1) && $(SRC_RTEMS)/configure \
	    --target=$(ARCH)-rtems$(RTEMS_VERSION) \
	    --prefix=$(PREFIX) \
	    --enable-posix \
	    --enable-rtemsbsp=$(BSP_GRISP1) \
	    --enable-maintainer-mode \
	    --disable-networking \
	    --disable-tests \
	    $(EXTRA_BSP_OPTS) \
	    --enable-chip=same70q21 \
	    --enable-sdram=is42s16320f-7bl \
	    ATSAM_CONSOLE_DEVICE_TYPE=1 \
	    ATSAM_CONSOLE_DEVICE_INDEX=2 \
	    ATSAM_MEMORY_QSPIFLASH_SIZE=0x0 \
	    ATSAM_MEMORY_NOCACHE_SIZE=0x8000
	cd $(BUILD_BSP_GRISP1) && make -j $(NUMCORE)
	cd $(BUILD_BSP_GRISP1) && make -j $(NUMCORE) install

.PHONY: bsp.mk
#H Build a Makefile helper for the applications.
bsp.mk: $(PREFIX)/make/custom/$(BSP).mk $(PREFIX)/make/custom/$(BSP_GRISP1).mk
$(PREFIX)/make/custom/$(BSP).mk: src/bsp.mk
	cat $^ | sed \
	    -e "s/##RTEMS_API##/$(RTEMS_VERSION)/g" \
	    -e "s/##RTEMS_BSP##/$(BSP)/g" \
	    -e "s/##RTEMS_CPU##/$(ARCH)/g" \
	    > $@
$(PREFIX)/make/custom/$(BSP_GRISP1).mk: src/bsp.mk
	cat $^ | sed \
	    -e "s/##RTEMS_API##/$(RTEMS_VERSION)/g" \
	    -e "s/##RTEMS_BSP##/$(BSP_GRISP1)/g" \
	    -e "s/##RTEMS_CPU##/$(ARCH)/g" \
	    > $@

.PHONY: libbsd
#H Build and install libbsd.
libbsd:
	rm -rf $(SRC_LIBBSD)/build
	cd $(SRC_LIBBSD) && ./waf configure \
	    --prefix=$(PREFIX) \
	    --rtems-bsps=$(ARCH)/$(BSP),$(ARCH)/$(BSP_GRISP1) \
	    --enable-warnings \
	    --optimization=$(OPTIMIZATION) \
	    --buildset=$(LIBBSD_BUILDSET) \
	    --rtems-version=$(RTEMS_VERSION)
	# Workaround for GRiSP1
	[ ! -e "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds.org" ] && \
		mv "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds" \
		    "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds.org" || \
		true
	cp "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds.sdram" \
	    "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds"
	# End of workaround for GRiSP1
	cd $(SRC_LIBBSD) && ./waf
	cd $(SRC_LIBBSD) && ./waf install
	# Workaround for GRiSP1
	cp "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds.org" \
	    "$(PREFIX)/$(TARGET)/$(BSP_GRISP1)/lib/linkcmds"
	# End of workaround for GRiSP1

.PHONY: libgrisp
#H Build and install libgrisp.
libgrisp:
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP) -C $(SRC_LIBGRISP) install
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP_GRISP1) -C $(SRC_LIBGRISP) install

.PHONY: libinih
#H Build and install libinih
libinih:
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP) -C $(SRC_LIBINIH) clean install
	make RTEMS_ROOT=$(PREFIX) RTEMS_BSP=$(BSP_GRISP1) -C $(SRC_LIBINIH) clean install

.PHONY: fdt
#H Build the flattened device tree.
fdt:
	make PREFIX=$(PREFIX) CPP=$(TARGET)-cpp -C fdt clean install

.PHONY: barebox
barebox: barebox-build barebox-install

.PHONY: barebox-install
# Install the bootloader
barebox-install:
	mkdir -p $(PREFIX)/barebox
	cp $(MAKEFILE_DIR)/barebox/barebox-phytec-phycore-imx6ull-emmc-512mb.img  $(PREFIX)/barebox

.PHONY: barebox-build
#H Build the bootloader
barebox-build:

ifneq ($(UNAME),Linux)
	$(error Barebox can only be built on Linux)
endif
	cd $(SRC_BAREBOX) && rm -f .config
	cd $(SRC_BAREBOX) && rm -f .env-extra
	cd $(SRC_BAREBOX) && ln -s $(MAKEFILE_DIR)/barebox/config .config
	cd $(SRC_BAREBOX) && ln -s $(MAKEFILE_DIR)/barebox/env .env-extra
	cd $(SRC_BAREBOX) && \
		[ -f $(MAKEFILE_DIR)/barebox/grisp2-state.patch ] && \
		if ! patch -R -p1 -s -f --dry-run < $(MAKEFILE_DIR)/barebox/grisp2-state.patch; then \
			patch -f -p1 < $(MAKEFILE_DIR)/barebox/grisp2-state.patch; \
		fi
	cd $(SRC_BAREBOX) && make ARCH=$(ARCH) CROSS_COMPILE=$(TARGET)- -j$(NUMCORE)
	cp $(SRC_BAREBOX)/images/barebox-phytec-phycore-imx6ull-emmc-512mb.img barebox

.PHONY: openocd
#H Build OpenOCD for debugging
openocd:
	# Note: Use a hack to _not_ use the RTEMS tools environment. The RTEMS
	# aclocal version isn't compatible with OpenOCD
	cd $(SRC_OPENOCD) && PATH="$(ORGPATH)" ./bootstrap
	cd $(SRC_OPENOCD) && PATH="$(ORGPATH)" ./configure --prefix="$(PREFIX)" \
	    --enable-ftdi \
	    --disable-stlink \
	    --disable-ti-icdi \
	    --disable-ulink \
	    --disable-usb-blaster-2 \
	    --disable-vsllink \
	    --disable-osbdm \
	    --disable-opendous \
	    --disable-aice \
	    --disable-usbprog \
	    --disable-rlink \
	    --disable-armjtagew \
	    --disable-cmsis-dap \
	    --disable-usb-blaster \
	    --disable-presto \
	    --disable-openjtag \
	    --disable-jlink \
	    --disable-parport \
	    --disable-parport-ppdev \
	    --disable-parport-giveio \
	    --disable-jtag_vpi \
	    --disable-amtjtagaccel \
	    --disable-zy1000-master \
	    --disable-zy1000 \
	    --disable-ioutil \
	    --disable-ep93xx \
	    --disable-at91rm9200 \
	    --disable-bcm2835gpio \
	    --disable-gw16012 \
	    --disable-oocd_trace \
	    --disable-buspirate \
	    --disable-sysfsgpio \
	    --disable-minidriver-dummy \
	    --disable-remote-bitbang \
	    --disable-werror
	cd $(SRC_OPENOCD) && PATH="$(ORGPATH)" make -j$(NUMCORE) install

.PHONY: imx_uart
#H Build the imx_uart tool for loading a bootloader
imx_uart:
	make -C $(SRC_IMX_USB_LOADER) imx_uart
	mkdir -p '$(PREFIX)/etc/imx-loader.d'
	install -m644 $(SRC_IMX_USB_LOADER)/mx6ull_usb_work.conf '$(PREFIX)/etc/imx-loader.d/'
	mkdir -p '$(PREFIX)/bin'
	install -m755 $(SRC_IMX_USB_LOADER)/imx_uart '$(PREFIX)/bin/imx_uart'

.PHONY: cmake_toolchain_config
cmake_toolchain_config:
	cat $(CMAKE_TOOLCHAIN_TEMPLATE) | sed \
	    -e "s|##PREFIX##|$(PREFIX)|g" \
	    -e "s|##BSP##|$(BSP)|g" \
	    -e "s|##TARGET##|$(TARGET)|g" \
	    > $(CMAKE_TOOLCHAIN_CONFIG)

.PHONY: cryptoauthlib
cryptoauthlib: cmake_toolchain_config
	mkdir -p $(SRC_CRYPTOAUTHLIB)/build
	mkdir -p $(SRC_CRYPTOAUTHLIB)/install
	cd $(SRC_CRYPTOAUTHLIB)/build && \
		cmake \
			-DATCA_HAL_KIT_HID=OFF \
			-DATCA_HAL_KIT_BRIDGE=OFF \
			-DATCA_HAL_KIT_UART=OFF \
			-DATCA_HAL_I2C=ON \
			-DATCA_HAL_SPI=OFF \
			-DATCA_HAL_CUSTOM=OFF \
			-DATCA_PKCS11=OFF \
			-DATCA_OPENSSL=OFF \
			-DATCA_ATSHA204A_SUPPORT=OFF \
			-DATCA_ATSHA206A_SUPPORT=OFF \
			-DATCA_ATECC108A_SUPPORT=OFF \
			-DATCA_ATECC508A_SUPPORT=OFF \
			-DATCA_ATECC608_SUPPORT=ON \
			-DATCA_TA100_SUPPORT=OFF \
			-DATCA_ECC204_SUPPORT=OFF \
			-DATCA_BUILD_SHARED_LIBS=OFF \
			-DATCA_USE_ATCAB_FUNCTIONS=ON \
			-DATCA_PRINTF=OFF \
			-DUNIX=true \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_CONFIG) ..
	cd $(SRC_CRYPTOAUTHLIB)/build && \
		make DESTDIR=$(SRC_CRYPTOAUTHLIB)/install install && \
		cp -r $(SRC_CRYPTOAUTHLIB)/install/usr/lib/libcryptoauth.a \
			$(PREFIX)/$(TARGET)/$(BSP)/lib/ && \
		cp -r $(SRC_CRYPTOAUTHLIB)/install/usr/include/cryptoauthlib \
			$(PREFIX)/$(TARGET)/$(BSP)/lib/include/ && \
		touch $(PREFIX)/$(TARGET)/$(BSP)/lib/include/cryptoauthlib/atca_start_config.h && \
		touch $(PREFIX)/$(TARGET)/$(BSP)/lib/include/cryptoauthlib/atca_start_iface.h

OPENBLAS_FLAGS=\
			BINARY=32 \
			CC='$(MAKEFILE_DIR)rtems/$(RTEMS_VERSION)/bin/$(ARCH)-rtems$(RTEMS_VERSION)-gcc -DOS_EMBEDDED' \
			HOSTCC=gcc \
			TARGET=ARMV7 \
			NO_LAPACK=1 \
			NOFORTRAN=1 \
			NO_SHARED=1 \
			USE_THREAD=0 \
			ONLY_CBLAS=1

.PHONY: OpenBLAS
OpenBLAS:
	mkdir -p $(SRC_OPENBLAS)/install
	cd $(SRC_OPENBLAS) && \
		make $(OPENBLAS_FLAGS) && \
		make PREFIX=$(SRC_OPENBLAS)/install $(OPENBLAS_FLAGS) install
	cp -r $(SRC_OPENBLAS)/install/lib/libopenblas.a \
			$(PREFIX)/$(TARGET)/$(BSP)/lib/
	cp -r $(SRC_OPENBLAS)/install/include/* \
			$(PREFIX)/$(TARGET)/$(BSP)/lib/include

.PHONY: demo
#H Build the demo application.
demo:
	make -C demo
	RTEMS_BSP=$(BSP_GRISP1) make -C demo

.PHONY: demo-clean
#H Clean the demo application.
demo-clean:
	make -C demo clean
	RTEMS_BSP=$(BSP_GRISP1) make -C demo clean

.PHONY: shell
#H Start a shell with the environment for building for example the RTEMS BSP.
shell:
	$(SHELL)

.PHONY: package
package:
	mkdir -p "${PACKAGE_DIR}"
	cd "${PREFIX}" &&  tar -czf "../../${PACKAGE_DIR}/${PACKAGE_PREFIX}_${OS_NAME}_${OS_VERSION}_${GRISP_TOOLCHAIN_REVISION}.tar.gz" *
