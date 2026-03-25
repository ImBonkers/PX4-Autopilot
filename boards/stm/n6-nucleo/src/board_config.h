/****************************************************************************
 * STM32N6 NUCLEO Board Configuration
 * Based on NUCLEO-N657X0-Q board (Cortex-M55 @ 800MHz)
 *
 * Uses native STM32N6 NuttX chip support.
 ****************************************************************************/

#pragma once

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

/****************************************************************************************************
 * Definitions — must come BEFORE board_common.h (which uses these macros)
 ****************************************************************************************************/

/* Board Configuration */
#define BOARD_HAS_FSBL                  1
#define BOARD_HAS_EXTERNAL_MEMORY       1

/* No onboard flash, no BBSRAM, no SDIO */
#define FLASH_BASED_PARAMS              0

/* Board HW versioning — simple stub */
#define BOARD_HAS_HW_VERSIONING         1

/* Enable console buffer for dmesg */
#define BOARD_ENABLE_CONSOLE_BUFFER     1

/* LEDs - NUCLEO-N657X0-Q (active LOW) */
/* PG0 = Green, PG8 = Blue, PG10 = Red (broken in DEV mode) */
#define GPIO_nLED_GREEN                 (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_SET | GPIO_PORTG | GPIO_PIN0)
#define GPIO_nLED_BLUE                  (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_SET | GPIO_PORTG | GPIO_PIN8)
#define GPIO_nLED_RED                   (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_SET | GPIO_PORTG | GPIO_PIN10)

#define BOARD_HAS_CONTROL_STATUS_LEDS   1
#define BOARD_OVERLOAD_LED              LED_RED
#define BOARD_ARMED_LED                 LED_BLUE
#define BOARD_ARMED_STATE_LED           LED_BLUE

/* Button - PC13 (active HIGH) */
#define GPIO_BTN_USER                   (GPIO_INPUT | GPIO_FLOAT | GPIO_PORTC | GPIO_PIN13)

/* UART Configuration */
/* USART1: PE5(TX)/PE6(RX) AF7 — ST-Link VCP console */
#define SERIAL_DEVICE_CONSOLE           "/dev/ttyS0"

/* SPI Bus — SPI5 only on Nucleo */
#define PX4_SPI_BUS_SENSORS             5

/* I2C Buses */
#define PX4_I2C_BUS_EXPANSION           1
#define PX4_I2C_BUS_EXPANSION1          2

/* No SPI devices on this board */

/* I2C bus configuration */
#define BOARD_NUMBER_I2C_BUSES          2
#define BOARD_I2C_BUS_CLOCK_INIT        {100000, 100000}

/* External Memory Configuration */
/* MX25UM51245G 64MB via XSPI2 at 0x70000000 */
#define BOARD_XSPI_MEMORY_SIZE          (64 * 1024 * 1024)

/****************************************************************************************************
 * Include board_common.h AFTER all board-specific defines
 ****************************************************************************************************/

#include <px4_platform_common/board_common.h>
