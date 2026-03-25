/****************************************************************************
 * STM32N6 NUCLEO Board I2C Configuration
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/i2c.h>
#include <px4_arch/i2c_hw_description.h>

#include <stdint.h>
#include <stdbool.h>

#include <nuttx/i2c/i2c_master.h>
#include <arch/board/board.h>

#include "board_config.h"

constexpr px4_i2c_bus_t px4_i2c_buses[I2C_BUS_MAX_BUS_ITEMS] = {
	initI2CBusExternal(1),
	initI2CBusExternal(2),
};
