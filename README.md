# GRiSP2 RTEMS Toolchain

:warning: This repository is still work in progress. Some stuff might is still
broken and some stuff won't work like expected. Use at your own risk. :warning:

This repository contains the basic tools and libraries to get an RTEMS system
running on GRiSP2. This includes:

* A `Makefile` that collects commands for building everything in this
  repository.
* The `rtems-source-builder` for building the toolchain (as a submodule).
* The bootloader `barebox` (as a submodule) and the necessary configuration.
* RTEMS and rtems-libbsd (as a submodule).
* A support library for some small GRiSP specific tasks `libgrisp` (as a
  submodule).
* A FDT (tested only with RTEMS)
* A RTEMS demo application.
* Some debugger scripts for a Lauterbach debugger (`t32`).

## How to Build

For building the basic stuff do a

    make install

This will build the toolchain, RTEMS, necessary libraries, the FDT, the
bootloader and basically everything that you need to create a RTEMS application
for the GRiSP2. Most of it will be installed to the `rtems/5` directory.
Exception: fdt and bootloader. The fdt will be at
`fdt/b-dtb/imx6ull-grisp2.dtb`. The bootloader image will be placed at
`external/barebox/images/barebox-phytec-phycore-imx6ull-grisp2.img`.

To build the demo application use a

    make demo

in the project root directory.

## How to Start an Application

The bootloader checks a number of boot devices. Among them is the SD-Card and
the eMMC. For these two the bootloader searches for an application image called
`zImage` and a device tree image called `oftree`. If these two files are found
the application will be booted.

For example to start the demo application you can use a FAT formatted SD-Card,
copy `fdt/b-dtb/imx6ull-grisp2.dtb` to `<SD-Path>/oftree` and copy
`demo/b-imx7/demo.zImage` to `<SD-Path>/zImage` and put the SD card in your
GRiSP. The bootloader will start this application.

Note that the eMMC has precedence. If the eMMC is written, the application from
eMMC will be started regardless of the SD content.

## Writing an Image to eMMC

One way to write an Image to eMMC is via the bootloader:

1. Make sure you have a DHCP server and a TFTP (announced via DHCP). You can for
   example use `dnsmasq` with the following config for that:

        interface=eth0
        bootp-dynamic
        domain=my-pc.eb.localhost
        dhcp-authoritative
        log-dhcp
        expand-hosts
        local=/my-pc.eb.localhost/
        
        enable-tftp=eth0
        tftp-root=/srv/tftpboot/
        
        ##############################
        # GRiSP2
        dhcp-host=50:2d:f4:14:26:0b,GRiSP2Proto009,set:grisp2
        dhcp-range=tag:grisp2,172.24.0.130,172.24.0.140,255.255.255.0,2m

1. Prepare an image to write on your PC. That can be a raw image copied from an
   SD card or created with a loop-device on a Linux PC. Make sure that the image
   isn't too big. It has to fit into the RAM of the bootloader. Copy it to your
   tftp server directory with some simple name (e.g. `grisp2.img`).
1. Connect the network cable and power up the GRiSP2.
1. Interrupt the boot loader when it outputs the `Hit m for menu or any key to
   stop autoboot:` line. You should drop into a shell with a `barebox@GRiSP2:/`
   prompt.
1. Type `mmc1.probe=1` to start the eMMC detection.
1. Type `dhcp` to get an IP address.
1. Type `tftp grisp.img`. If that doesn't work your server might hasn't been
   announced. In that case use a command like
   `global dev.eth0.serverip=172.24.0.99` to explicitly set your server and
   retry the tftp command.
1. Write the image to the emmc using `cp grisp.img /dev/mmc1`. Wait till the
   image is written.
1. Reset the system with `reset` on the shell. Barebox should now boot your
   application.
