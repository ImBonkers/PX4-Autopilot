/****************************************************************************
 * STM32N6 NUCLEO — PX4-specific stubs
 *
 * Board boot/bringup/peripherals come from the NuttX board files
 * (stm32_boot.c, stm32_bringup.c, stm32_appinit.c, etc.)
 * This file only provides symbols that PX4 needs but NuttX doesn't.
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Flash parameter stubs — STM32N6 has no internal flash for params.
 * TODO: use littlefs on XSPI2 for parameter storage.
 */

typedef struct { char n[4]; } flash_file_token_t;

const flash_file_token_t parameters_token = {
	.n = {'p', 'a', 'r', 'm'},
};

/* Empty BSON document: just the size field (5 bytes = minimum valid BSON) */
static uint8_t empty_bson[] = { 0x05, 0x00, 0x00, 0x00, 0x00 };

void parameter_flashfs_free(void) {}

int parameter_flashfs_write(flash_file_token_t token, uint8_t *buf,
			    size_t buf_size)
{
	/* Silently accept — no persistent storage yet. */
	return buf_size;
}

int parameter_flashfs_read(flash_file_token_t token, uint8_t **buf,
			   size_t *buf_size)
{
	/* Return empty BSON so param import succeeds with defaults */
	*buf = empty_bson;
	*buf_size = sizeof(empty_bson);
	return sizeof(empty_bson);
}

int parameter_flashfs_alloc(flash_file_token_t token, uint8_t **buf,
			    size_t *buf_size)
{
	/* Allocate a temporary buffer for param export.
	 * Caller will write BSON data and call parameter_flashfs_write.
	 */
	static uint8_t param_buf[4096];

	*buf = param_buf;

	if (*buf_size > sizeof(param_buf)) {
		*buf_size = sizeof(param_buf);
	}

	return 0; /* OK */
}

/* Latency tracking — PX4 perf counters need these */
#include <drivers/drv_hrt.h>
uint32_t latency_counters[LATENCY_BUCKET_COUNT + 1] = {};
const uint16_t latency_buckets[LATENCY_BUCKET_COUNT] = {
	1, 2, 5, 10, 20, 50, 100, 1000
};

/* Board HW info */
const char *board_get_hw_type_name(void)
{
	return "N6_NUCLEO";
}

int board_get_hw_version(void)
{
	return 0;
}

int board_get_hw_revision(void)
{
	return 0;
}

int board_determine_hw_info(void)
{
	return 0;
}

int board_get_uuid32(uint32_t *uuid_words)
{
	/* Return a dummy UUID — STM32N6 UID registers are RIF-protected */
	uuid_words[0] = 0x4E363030; /* "N600" */
	uuid_words[1] = 0;
	uuid_words[2] = 0;
	return 3;
}

/* No-op — mock sensors removed, let jMAVSim provide all HIL data */
void mock_hil_sensors(void) {}

/* Board GUID */
int board_get_px4_guid_formated(char *format_buffer, int size)
{
	strncpy(format_buffer, "STM32N6-NUCLEO", size);
	return 0;
}

int board_get_px4_guid(uint8_t guid[16])
{
	memset(guid, 0, 16);
	guid[0] = 0x06;  /* STM32N6 marker */
	return 16;
}
