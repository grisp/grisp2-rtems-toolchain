MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
RTEMS_ROOT ?= $(MAKEFILE_DIR)/../rtems/6
RTEMS_BSP ?= stm32u5-grisp-nano

include $(RTEMS_ROOT)/make/custom/$(RTEMS_BSP).mk

GRISP_BL_VERSION := $(shell git describe --dirty --always --match="nano")
CFLAGS += -DGRISP_BL_VERSION="\"$(GRISP_BL_VERSION)\""
CFLAGS += -O2 -g
LDFLAGS += -qnolinkcmds -T linkcmds.bootloader

APP = $(BUILDDIR)/nano-bootloader
APP_PIECES = $(wildcard *.c)
APP_OBJS = $(APP_PIECES:%.c=$(BUILDDIR)/%.o)
APP_DEPS = $(APP_PIECES:%.c=$(BUILDDIR)/%.d)

all: $(BUILDDIR) $(APP).exe

$(BUILDDIR):
	mkdir $(BUILDDIR)

$(APP).exe: $(APP_OBJS)
	$(CCLINK) $^ -lgrisp -linih -lbsd -lm -o $@

clean:
	rm -rf $(BUILDDIR)

-include $(APP_DEPS)
