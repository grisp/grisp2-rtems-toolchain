/*
 * Copyright (c) 2016-2020 embedded brains GmbH.  All rights reserved.
 *
 *  embedded brains GmbH
 *  Dornierstr. 4
 *  82178 Puchheim
 *  Germany
 *  <rtems@embedded-brains.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#undef EVENT_RECORDING
#define RTC_ENABLED

#include <assert.h>
#include <stdlib.h>

#include <rtems.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bdbuf.h>
#include <rtems/console.h>
#include <rtems/malloc.h>
#include <rtems/media.h>
#include <rtems/score/armv7m.h>
#include <rtems/shell.h>
#include <rtems/stringto.h>
#include <rtems/ftpd.h>
#include <machine/rtems-bsd-commands.h>
#ifdef RTC_ENABLED
#include <libchip/rtc.h>
#include <libchip/mcp7940m-rtc.h>
#endif
#ifdef EVENT_RECORDING
#include <rtems/record.h>
#include <rtems/recordserver.h>
#endif /* EVENT_RECORDING */

#include <bsp.h>
#ifdef LIBBSP_ARM_ATSAM_BSP_H
#define IS_GRISP1 1
#else
#define IS_GRISP2 1
#endif

#include <grisp.h>
#ifdef IS_GRISP1
#include <bsp/i2c.h>
#include <grisp/pin-config.h>
#endif /* IS_GRISP1 */
#include <grisp/led.h>
#include <grisp/init.h>
#include <grisp/eeprom.h>

#include "fragmented-read-test.h"
#include "sd-card-test.h"
#include "1wire.h"
#include "pmod_rfid.h"
#include "pmod_dio.h"

#define STACK_SIZE_INIT_TASK	(64 * 1024)
#define STACK_SIZE_SHELL	(64 * 1024)

#define PRIO_SHELL		150
#define PRIO_LED_TASK		(RTEMS_MAXIMUM_PRIORITY - 1)
#define PRIO_DHCP		(RTEMS_MAXIMUM_PRIORITY - 1)
#define PRIO_WPA		(RTEMS_MAXIMUM_PRIORITY - 1)

#define SPI_FDT_NAME "spi0"

#define CMD_SPI_MAX_LEN 32

const char *wpa_supplicant_conf = "/media/mmcsd-0-0/wpa_supplicant.conf";

#ifdef IS_GRISP1
const Pin atsam_pin_config[] = {GRISP_PIN_CONFIG};
const size_t atsam_pin_config_count = PIO_LISTSIZE(atsam_pin_config);
const uint32_t atsam_matrix_ccfg_sysio = GRISP_MATRIX_CCFG_SYSIO;
#endif /* IS_GRISP1 */

struct rtems_ftpd_configuration rtems_ftpd_configuration = {
	.priority = 100,
	.max_hook_filesize = 0,
	.port = 21,
	.hooks = NULL,
	.root = NULL,
	.tasks_count = 4,
	.idle = 5 * 60,
	.access = 0
};

static void
start_shell(void)
{
	rtems_status_code sc = rtems_shell_init(
		"SHLL",
		STACK_SIZE_SHELL,
		PRIO_SHELL,
		CONSOLE_DEVICE_NAME,
		false,
		true,
		NULL
	);
	assert(sc == RTEMS_SUCCESSFUL);
}

static void
create_wlandev(void)
{
	int exit_code;
	char *ifcfg_check[] = {
		"ifconfig",
		"wlan0",
		NULL
	};
	char *ifcfg_create[] = {
		"ifconfig",
		"wlan0",
		"create",
		"wlandev",
		"rtwn0",
		"down",
		NULL
	};
	char *ifcfg_up[] = {
		"ifconfig",
		"wlan0",
		"up",
		NULL
	};

	/* Check if device exists */
	exit_code = rtems_bsd_command_ifconfig(
	    RTEMS_BSD_ARGC(ifcfg_check), ifcfg_check);
	if (exit_code != EXIT_SUCCESS) {
		/* Create it if not */
		exit_code = rtems_bsd_command_ifconfig(
		    RTEMS_BSD_ARGC(ifcfg_create), ifcfg_create);
		if(exit_code != EXIT_SUCCESS) {
			printf("ERROR while creating wlan0.\n");
		}
		sleep(1);
		exit_code = rtems_bsd_command_ifconfig(
		    RTEMS_BSD_ARGC(ifcfg_up), ifcfg_up);
		if(exit_code != EXIT_SUCCESS) {
			printf("ERROR while setting wlan0 up.\n");
		}
	}
}

static void
led_task(rtems_task_argument arg)
{
	unsigned state = 0;

	(void)arg;

	while(true) {
		bool r1 = (state & (1 << 0)) != 0;
		bool g1 = (state & (1 << 1)) != 0;
		bool b1 = (state & (1 << 2)) != 0;
		state += 1;
		bool r2 = (state & (1 << 0)) != 0;
		bool g2 = (state & (1 << 1)) != 0;
		bool b2 = (state & (1 << 2)) != 0;

		grisp_led_set_som((state & 1) != 0);
		grisp_led_set1(r1, g1, b1);
		grisp_led_set2(r2, g2, b2);

		rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(250));
	}
}

static void
init_led(void)
{
	rtems_status_code sc;
	rtems_id id;

	sc = rtems_task_create(
		rtems_build_name('L', 'E', 'D', ' '),
		PRIO_LED_TASK,
		RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_DEFAULT_MODES,
		RTEMS_DEFAULT_ATTRIBUTES,
		&id
	);
	assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_start(id, led_task, 0);
	assert(sc == RTEMS_SUCCESSFUL);
}

static int
command_startftp(int argc, char *argv[])
{
	rtems_status_code sc;

	(void) argc;
	(void) argv;

	sc = rtems_initialize_ftpd();
	if(sc == RTEMS_SUCCESSFUL) {
		printf("FTP started.\n");
	} else {
		printf("ERROR: FTP could not be started.\n");
	}

	return 0;
}

rtems_shell_cmd_t rtems_shell_STARTFTP_Command = {
	"startftp",          /* name */
	"startftp",          /* usage */
	"net",               /* topic */
	command_startftp,    /* command */
	NULL,                /* alias */
	NULL,                /* next */
	0, 0, 0
};

static void
Init(rtems_task_argument arg)
{
	rtems_status_code sc;
	int rv;
	struct grisp_eeprom eeprom = {0};

	(void)arg;

	puts("\nGRiSP2 RTEMS Demo\n");

#ifdef IS_GRISP1
	rv = atsam_register_i2c_0();
	assert(rv == 0);
#endif
#ifdef IS_GRISP2
	if (grisp_is_industrialgrisp()) {
		/* Industrial GRiSP */
		rv = spi_bus_register_imx(GRISP_SPI_DEVICE,
				GRISP_INDUSTRIAL_SPI_ONBOARD_FDT_ALIAS);
		assert(rv == 0);
		rv = spi_bus_register_imx(GRISP_SPI_DEVICE "-pmod",
				GRISP_INDUSTRIAL_SPI_PMOD_FDT_ALIAS);
		assert(rv == 0);

		rv = i2c_bus_register_imx(GRISP_I2C0_DEVICE,
				GRISP_INDUSTRIAL_I2C_FDT_ALIAS);
		assert(rv == 0);
	} else {
		/* GRiSP2 */
		rv = spi_bus_register_imx(GRISP_SPI_DEVICE,
				GRISP_SPI_FDT_ALIAS);
		assert(rv == 0);

		rv = i2c_bus_register_imx("/dev/i2c-1", "i2c0");
		assert(rv == 0);

		rv = i2c_bus_register_imx("/dev/i2c-2", "i2c1");
		assert(rv == 0);
	}
#endif /* IS_GRISP2 */

#ifdef RTC_ENABLED
	setRealTimeToRTEMS();
#endif

	printf("Init EEPROM\n");
	grisp_eeprom_init();
	rv = grisp_eeprom_get(&eeprom);
#ifndef IS_GRISP1 /* On GRiSP1 the checksum hasn't been calculated correctly */
	if (rv == 0) {
#endif
		grisp_eeprom_dump(&eeprom);
#ifndef IS_GRISP1
	} else {
		printf("ERROR: Invalid EEPROM\n");
	}
#endif

	grisp_init_sd_card();
	grisp_init_lower_self_prio();
	grisp_init_libbsd();

	/* Wait for the SD card */
	sc = grisp_init_wait_for_sd();
	if (sc == RTEMS_SUCCESSFUL) {
		printf("SD: OK\n");
	} else {
		printf("ERROR: SD could not be mounted after timeout\n");
		grisp_led_set1(true, false, false);
	}

	sleep(1);
	grisp_init_dhcpcd(PRIO_DHCP);

	grisp_led_set2(false, false, true);
	if (!grisp_is_industrialgrisp()) {
		sleep(3);
		grisp_init_wpa_supplicant(wpa_supplicant_conf, PRIO_WPA,
		    create_wlandev);
	}

#ifdef EVENT_RECORDING
	rtems_record_start_server(10, 1234, 10);
	rtems_record_line();
#endif /* EVENT_RECORDING */

	init_led();
#ifdef IS_GRISP2
	if (grisp_is_industrialgrisp()) {
		pmod_rfid_init(GRISP_SPI_DEVICE, 0);
		pmod_dio_init(GRISP_SPI_DEVICE);
	} else {
		// uncomment for testing RFID
		//pmod_rfid_init(GRISP_SPI_DEVICE, 1);
	}
#endif /* IS_GRISP2 */
	start_shell();

	exit(0);
}

#ifdef RTC_ENABLED
static struct mcp7940m_rtc rtc_ctx =
	MCP7940M_RTC_INITIALIZER("/dev/i2c-1", 0x6f, false);

rtc_tbl RTC_Table[] = {
	MCP7940M_RTC_TBL_ENTRY("/dev/rtc", &rtc_ctx),
};

size_t RTC_Count = (sizeof(RTC_Table)/sizeof(rtc_tbl));
#endif

/*
 * Configure LibBSD.
 */
#include <grisp/libbsd-nexus-config.h>
#define RTEMS_BSD_CONFIG_TERMIOS_KQUEUE_AND_POLL
#define RTEMS_BSD_CONFIG_INIT

#include <machine/rtems-bsd-config.h>

/*
 * Configure RTEMS.
 */
#define CONFIGURE_MICROSECONDS_PER_TICK 10000

#ifdef RTC_ENABLED
#define CONFIGURE_APPLICATION_NEEDS_RTC_DRIVER
#endif
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#define CONFIGURE_FILESYSTEM_DOSFS
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 32

#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1

#define CONFIGURE_INIT_TASK_STACK_SIZE STACK_SIZE_INIT_TASK
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (32 * 1024)
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 4
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (1 * 1024 * 1024)
#define CONFIGURE_BDBUF_READ_AHEAD_TASK_PRIORITY 97
#define CONFIGURE_SWAPOUT_TASK_PRIORITY 97

//#define CONFIGURE_STACK_CHECKER_ENABLED
#ifdef EVENT_RECORDING
#define CONFIGURE_RECORD_EXTENSIONS_ENABLED
#define CONFIGURE_RECORD_PER_PROCESSOR_ITEMS (128 * 1024)
#define CONFIGURE_RECORD_FATAL_DUMP_BASE64
#endif /* EVENT_RECORDING */

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT

#include <rtems/confdefs.h>

/*
 * Configure Shell.
 */
#include <rtems/netcmds-config.h>
#include <bsp/irq-info.h>
#define CONFIGURE_SHELL_COMMANDS_INIT

#define CONFIGURE_SHELL_USER_COMMANDS \
  &bsp_interrupt_shell_command, \
  &rtems_shell_ARP_Command, \
  &rtems_shell_PFCTL_Command, \
  &rtems_shell_PING_Command, \
  &rtems_shell_IFCONFIG_Command, \
  &rtems_shell_ROUTE_Command, \
  &rtems_shell_NETSTAT_Command, \
  &rtems_shell_DHCPCD_Command, \
  &rtems_shell_HOSTNAME_Command, \
  &rtems_shell_SYSCTL_Command, \
  &rtems_shell_VMSTAT_Command, \
  &rtems_shell_WLANSTATS_Command, \
  &rtems_shell_STARTFTP_Command, \
  &rtems_shell_BLKSTATS_Command, \
  &rtems_shell_WPA_SUPPLICANT_Command, \
  &rtems_shell_WPA_SUPPLICANT_FORK_Command, \
  &shell_PATTERN_FILL_Command, \
  &shell_PATTERN_CHECK_Command, \
  &shell_1wiretemp_command, \
  &shell_FRAGMENTED_READ_TEST_Command

#define CONFIGURE_SHELL_COMMANDS_ALL

#include <rtems/shellconfig.h>
