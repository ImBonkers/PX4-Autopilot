#pragma once

#include <px4_platform/micro_hal.h>

__BEGIN_DECLS

/* Define N6 SOC arch ID — no existing enum, reuse a unique value */
#define PX4_SOC_ARCH_ID             0x0006  /* STM32N6 */

/* STM32N6 NuttX chip headers */
#include <stm32_gpio.h>
#include <stm32_spi.h>
#include <stm32_i2c.h>

/* STM32N6 has no internal flash */
#define PX4_FLASH_BASE  0x70000000  /* External flash via XSPI2 */

#include <chip.h>
#include <arm_internal.h>

#define PX4_NUMBER_I2C_BUSES 2

/* UUID — STM32N6 has 96-bit unique ID */
#define PX4_CPU_UUID_BYTE_LENGTH                12
#define PX4_CPU_UUID_WORD32_LENGTH              (PX4_CPU_UUID_BYTE_LENGTH/sizeof(uint32_t))
#define PX4_CPU_MFGUID_BYTE_LENGTH              PX4_CPU_UUID_BYTE_LENGTH

#define PX4_CPU_UUID_WORD32_UNIQUE_H            2
#define PX4_CPU_UUID_WORD32_UNIQUE_M            1
#define PX4_CPU_UUID_WORD32_UNIQUE_L            0

#define PX4_CPU_UUID_WORD32_FORMAT_SIZE         (PX4_CPU_UUID_WORD32_LENGTH-1+(2*PX4_CPU_UUID_BYTE_LENGTH)+1)
#define PX4_CPU_MFGUID_FORMAT_SIZE              ((2*PX4_CPU_MFGUID_BYTE_LENGTH)+1)

#define PX4_BUS_OFFSET       0
#define px4_spibus_initialize(bus_num_1based)   stm32_spibus_initialize(bus_num_1based)

#define px4_i2cbus_initialize(bus_num_1based)   stm32_i2cbus_initialize(bus_num_1based)
#define px4_i2cbus_uninitialize(pdev)           stm32_i2cbus_uninitialize(pdev)

#define px4_arch_configgpio(pinset)             stm32_configgpio(pinset)
#define px4_arch_unconfiggpio(pinset)           stm32_unconfiggpio(pinset)
#define px4_arch_gpioread(pinset)               stm32_gpioread(pinset)
#define px4_arch_gpiowrite(pinset, value)       stm32_gpiowrite(pinset, value)
#define px4_arch_gpiosetevent(pinset,r,f,e,fp,a)  stm32_gpiosetevent(pinset,r,f,e,fp,a)

#define PX4_MAKE_GPIO_INPUT(gpio) (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | (GPIO_INPUT|GPIO_PULLUP))
#define PX4_MAKE_GPIO_EXTI(gpio) (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | (GPIO_EXTI|GPIO_INPUT|GPIO_PULLUP))
#define PX4_MAKE_GPIO_OUTPUT_CLEAR(gpio) (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_2MHz|GPIO_OUTPUT_CLEAR))
#define PX4_MAKE_GPIO_OUTPUT_SET(gpio) (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_2MHz|GPIO_OUTPUT_SET))

#define PX4_GPIO_PIN_OFF(def) (((def) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | (GPIO_INPUT|GPIO_FLOAT|GPIO_SPEED_2MHz))

#define TIMER_HRT_CYCLES_PER_US (STM32_HCLK_FREQUENCY/1000000)
#define TIMER_HRT_CYCLES_PER_MS (STM32_HCLK_FREQUENCY/1000)

/* D-cache alignment for DMA */
#if defined(CONFIG_ARMV8M_DCACHE)
#  define PX4_ARCH_DCACHE_ALIGNMENT 32
#  define px4_cache_aligned_data() aligned_data(32)
#  define px4_cache_aligned_alloc(s) memalign(32,(s))
#else
#  define px4_cache_aligned_data()
#  define px4_cache_aligned_alloc malloc
#endif

__END_DECLS
