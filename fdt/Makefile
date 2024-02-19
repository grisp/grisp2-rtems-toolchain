# Note: $(PWD) doesn't work together with -C option of make
MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

DTS = imx6ul-grisp2.dts imx6ull-industrialgrisp.dts
SRC_FDT = $(MAKEFILE_DIR)/../external/fdt
FDT_SYS = $(SRC_FDT)/sys
FDT_SCRIPTS = $(FDT_SYS)/tools/fdt
BUILDDIR = $(MAKEFILE_DIR)/b-dtb

# necessary for the make_dtb.sh
export DTC = dtc
export MACHINE = arm

DTB = $(DTS:%.dts=$(BUILDDIR)/%.dtb)

all: $(DTB)

install: all
	mkdir -p $(PREFIX)/fdt
	cp $(DTS) $(PREFIX)/fdt/
	cp $(DTB) $(PREFIX)/fdt/

$(BUILDDIR):
	mkdir $(BUILDDIR)

$(BUILDDIR)/%.dtb: %.dts $(BUILDDIR)
	sh -x $(FDT_SCRIPTS)/make_dtb.sh $(FDT_SYS) $< $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
