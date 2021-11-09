MAKEFILE_DIR = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
RTEMS_ROOT ?= $(MAKEFILE_DIR)/../rtems/5
RTEMS_BSP ?= imx7

include $(RTEMS_ROOT)/make/custom/$(RTEMS_BSP).mk

CFLAGS += -O0
ifeq ($(RTEMS_BSP),atsamv)
LDFLAGS += -qnolinkcmds -T linkcmds.sdram
endif

APP = $(BUILDDIR)/demo
APP_PIECES = $(wildcard *.c)
APP_OBJS = $(APP_PIECES:%.c=$(BUILDDIR)/%.o)
APP_DEPS = $(APP_PIECES:%.c=$(BUILDDIR)/%.d)

all: $(BUILDDIR) $(APP).exe $(APP).zImage

$(BUILDDIR):
	mkdir $(BUILDDIR)

$(APP).exe: $(APP_OBJS)
	$(CCLINK) $^ -lgrisp -lftpd -lbsd -lm -o $@

$(APP).bin: $(APP).exe
	$(OBJCOPY) -O binary $^ $@

$(APP).zImage: $(APP).bin
	rm -f $<.gz
	gzip -k -9 $<
	mkimage.py -A arm -O linux -T kernel -a 0x80200000 -e 0x80200000 -n RTEMS -d $<.gz $@

bin: all $(APP).bin

clean:
	rm -rf $(BUILDDIR)

debugoutput:
	echo $(PATH)

-include $(APP_DEPS)
