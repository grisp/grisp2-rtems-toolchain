# GRiSP 2 Boot Loader Update

Use the following instructions to update the Barebox boot loader for GRiSP 2 to the latest version.

1. Copy `barebox/barebox-phytec-phycore-imx6ull-emmc-512mb.img` from this repository to a FAT32 formatted SD card (assuming the path `/Volumes/GRISP` to the SD card):

   ```sh
   $ cd grisp2-rtems-toolchain
   $ cp barebox/barebox-phytec-phycore-imx6ull-emmc-512mb.img /Volumes/GRISP/
   $ diskutil unmount /Volumes/GRISP # macOS
   ```

2. Connect to the Erlang node running on your board ([over a Serial](https://github.com/grisp/grisp/wiki/Connecting-over-Serial) or [remotely via Ethernet](https://github.com/grisp/grisp/wiki/Connecting-over-Serial)), press the RESET button and abort the standard boot sequence by pressing `<Enter>` at the prompt:

   ```
   barebox 2019.01.0-bsp-yocto-i.mx6ul-pd19.1.1 #5 Thu Aug 26 14:45:01 CEST 2021
   
   Board: PHYTEC phyCORE-i.MX6 Ultra Light SOM with eMMC
   detected i.MX6 ULL revision 1.1
   i.MX reset reason POR (SRSR: 0x00000001)
   i.MX6 ULL unique ID: 5988f541131f59d7
   mdio_bus: miibus0: probed
   eth0: got preset MAC address: 50:2d:f4:23:a1:cb
   imx-usb 2184200.usb@2184200.of: USB EHCI 1.00
   imx-esdhc 2190000.usdhc@2190000.of: registered as mmc0
   imx-esdhc 2194000.usdhc@2194000.of: registered as mmc1
   netconsole: registered as netconsole-1
   phySOM-i.MX6: Using environment in MMC
   malloc space: 0x8fe7d100 -> 0x9fcfa1ff (size 254.5 MiB)
   mmc1: detected MMC card version 5.0
   mmc1: registered mmc1.boot0
   mmc1: registered mmc1.boot1
   mmc1: registered mmc1
   running /env/bin/init...
   
   Hit m for menu or any key to stop autoboot:    3         <PRESS ENTER HERE>
   
   type exit to get to the menu
   ...:/
   ```

3. Verify that the new boot loader image is on the SD card:

   ```
   ...:/ ls /mnt/mmc 
   mmc0: detected SD card version 2.0
   mmc0: registered mmc0
   .
   ..
   barebox-phytec-phycore-imx6ull-emmc-512mb.img
   grisp.ini
   loader
   myrelease
   ```

4. Boot from that boot loader to verify that it works:

   ```
   ...:/ bootm /mnt/mmc/barebox-phytec-phycore-imx6ull-emmc-512mb.img 
   
   Loading ARM barebox image '/mnt/mmc/barebox-phytec-phycore-imx6ull-emmc-512mb.img'
   commandline: consoleblank=0 console=ttymxc0,115200n8   rootwait ro fsck.repair=yes
   Starting kernel in secure mode
   
   barebox 2019.01.0-bsp-yocto-i.mx6ul-pd19.1.1 #5 Thu Aug 26 14:45:01 CEST 2021
   ...
   ```

   Make sure booting into Erlang from the new bootloader works completely. The complete testing boot sequence should be as follows: **Old Bootloader** → **New Bootloader** → **Erlang** (or other software that you want to boot).
   
5. Reset the board (into the old bootloader, or load the new bootloader again) and flash the new boot loader (press <kbd>y</kbd> when prompted):

   ```
   ...:/ barebox_update /mnt/mmc/barebox-phytec-phycore-imx6ull-emmc-512mb.img 
   barebox_update /mnt/mmc/barebox-phytec-phycore-imx6ull-emmc-512mb.img
   mmc0: detected SD card version 2.0
   mmc0: registered mmc0
   Image Metadata:
     build: #5 Thu Aug 26 14:45:01 CEST 2021
     release: 2019.01.0-bsp-yocto-i.mx6ul-pd19.1.1
     parameter: memsize=512
   update barebox from /mnt/mmc/barebox-phytec-phycore-imx6ull-emmc-512mb.img using handler mmc1 to /dev/mmc1 (y/n)? y 
   updating barebox...
   update succeeded
   ...:/
   ```

6. Reset the board. It should now boot into the new boot loader. The environment might look different, execute `defaultenv -r` to restore the default environment for that particular barebox binary. Non-volatile variables (`nv`) are not affected.
