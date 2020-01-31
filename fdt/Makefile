# Note: $(PWD) doesn't work together with -C option of make
MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

BOARD = imx6ull-grisp2
SRC_FDT = $(MAKEFILE_DIR)/../external/fdt
FDT_SYS = $(SRC_FDT)/sys
FDT_SCRIPTS = $(FDT_SYS)/tools/fdt
BUILDDIR = $(MAKEFILE_DIR)/b-dtb

# necessary for the make_dtb.sh
export DTC = dtc
export MACHINE = arm

DTB = $(BUILDDIR)/$(BOARD).dtb

all: $(DTB)

$(BUILDDIR):
	mkdir $(BUILDDIR)

$(DTB): $(BOARD).dts $(BUILDDIR)
	$(FDT_SCRIPTS)/make_dtb.sh $(FDT_SYS) $< $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
