/****************************************************************************
 * STM32N6 NUCLEO Board SPI & I2C Bus Configuration
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/spi.h>
#include <px4_platform_common/i2c.h>
#include <px4_arch/i2c_hw_description.h>

#include <stdint.h>
#include <stdbool.h>

#include <nuttx/spi/spi.h>
#include <arch/board/board.h>

#include "board_config.h"

/* No onboard SPI sensors on Nucleo */
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {};

/* I2C buses — both external */
constexpr px4_i2c_bus_t px4_i2c_buses[I2C_BUS_MAX_BUS_ITEMS] = {
	initI2CBusExternal(1),
	initI2CBusExternal(2),
};

#ifdef CONFIG_STM32N6_SPI5
void stm32_spi5select(struct spi_dev_s *dev, uint32_t devid, bool selected)
{
}

uint8_t stm32_spi5status(struct spi_dev_s *dev, uint32_t devid)
{
	return SPI_STATUS_PRESENT;
}
#endif
