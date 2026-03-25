/****************************************************************************
 * boards/arm/stm32n6/nucleo-n657x0-q/src/stm32_bringup.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/mount.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>

#include <nuttx/board.h>
#include <nuttx/spi/spi_transfer.h>
#include <nuttx/i2c/i2c_master.h>
#ifdef CONFIG_CDCACM
#include <nuttx/usb/cdcacm.h>
#endif
#ifdef CONFIG_RTC_DRIVER
#include <nuttx/timers/rtc.h>
#include "stm32_rtc.h"
#endif

#include <nuttx/cache.h>
#include "arm_internal.h"
#include "stm32_rcc.h"
#include "stm32_gpio.h"
#include "stm32_spi.h"
#include "stm32_i2c.h"
#ifdef CONFIG_STM32N6_ADC
#include "stm32_adc.h"
#endif
#ifdef CONFIG_STM32N6_PWM
#include "stm32_pwm.h"
#endif
#ifdef CONFIG_STM32N6_IWDG
#include "stm32_wdg.h"
#endif
#ifdef CONFIG_STM32N6_XSPI
#include "stm32_xspi.h"
#endif
#ifdef CONFIG_STM32N6_NPU
#include "stm32_npu.h"
#endif
#include "nucleo-n657x0-q.h"

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_bringup
 *
 * Description:
 *   Perform architecture-specific initialization
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=y :
 *     Called from board_late_initialize().
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=n && CONFIG_BOARDCTL=y :
 *     Called from the NSH library
 *
 ****************************************************************************/

int stm32_bringup(void)
{
  int ret;

#ifdef CONFIG_STM32N6_NPU
  /* NPU driver: HW init + register /dev/npu0.
   * CACHEAXI is disabled at end of HW init to avoid breaking XSPI2
   * indirect mode.  It's re-enabled only during NPU inference.
   */

  ret = stm32_npu_setup();
  if (ret < 0)
    {
      ferr("ERROR: stm32_npu_setup failed: %d\n", ret);
    }
#endif

  /* Turn on user LEDs: LD6 (green/PG0) and LD7 (blue/PG8) work in DEV mode.
   * LD5 (red/PG10) does not light in DEV mode despite correct GPIO config
   * (confirmed: MODER=output, ODR=LOW, IDR=LOW).  Likely ST-LINK V3EC
   * interference via SWD on PG10 in DEV boot mode.
   */

  stm32_gpiowrite(GPIO_LD6, true);
  stm32_gpiowrite(GPIO_LD7, true);

#ifdef CONFIG_FS_PROCFS
  /* Mount the procfs file system */

  ret = mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      ferr("ERROR: Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_RTC_DRIVER
  /* Register /dev/rtc0 */

  {
    struct rtc_lowerhalf_s *lower = stm32_rtc_lowerhalf();
    if (lower != NULL)
      {
        ret = rtc_initialize(0, lower);
        if (ret < 0)
          {
            ferr("ERROR: rtc_initialize failed: %d\n", ret);
          }
      }
  }
#endif

#ifdef CONFIG_STM32N6_ADC1
  /* Initialize ADC1 and register as /dev/adc0 */

  ret = stm32_adc_setup();
  if (ret < 0)
    {
      ferr("ERROR: stm32_adc_setup failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_STM32N6_PWM
  /* Initialize PWM and register as /dev/pwm0 */

  ret = stm32_pwm_setup();
  if (ret < 0)
    {
      ferr("ERROR: stm32_pwm_setup failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_STM32N6_IWDG
  stm32_iwdginitialize("/dev/watchdog0", STM32_LSI_FREQUENCY);
#endif

#ifdef CONFIG_STM32N6_SPI5
  /* Initialize SPI5 and register as /dev/spi5 for the spi tool */

  {
    struct spi_dev_s *spi5 = stm32_spibus_initialize(5);
    if (spi5 != NULL)
      {
        ret = spi_register(spi5, 5);
        if (ret < 0)
          {
            ferr("ERROR: Failed to register /dev/spi5: %d\n", ret);
          }
      }
  }
#endif


#ifdef CONFIG_STM32N6_I2C1
  /* Initialize I2C1 and register as /dev/i2c1 for the i2c tool.
   * I2C1: PH9=SCL, PC1=SDA — morpho CN15 pin3(SCL), pin5(SDA).
   */

  {
    struct i2c_master_s *i2c1 = stm32_i2cbus_initialize(1);
    if (i2c1 != NULL)
      {
        ret = i2c_register(i2c1, 1);
        if (ret < 0)
          {
            ferr("ERROR: Failed to register /dev/i2c1: %d\n", ret);
          }
      }
  }
#endif

#ifdef CONFIG_STM32N6_I2C2
  /* Enable TCPP0203 USB Type-C PD controller on I2C2.
   * The chip has an enable pin on PA7 (push-pull, active HIGH).
   * Must be driven HIGH before I2C communication will succeed.
   */

  stm32_configgpio(GPIO_TCPP03_ENABLE);
  stm32_gpiowrite(GPIO_TCPP03_ENABLE, true);
  up_mdelay(10); /* TCPP03 needs time after enable */

  /* Initialize I2C2 and register as /dev/i2c2 for the i2c tool */

  {
    struct i2c_master_s *i2c2 = stm32_i2cbus_initialize(2);
    if (i2c2 != NULL)
      {
        ret = i2c_register(i2c2, 2);
        if (ret < 0)
          {
            ferr("ERROR: Failed to register /dev/i2c2: %d\n", ret);
          }

      }
  }

#endif

#ifdef CONFIG_STM32N6_XSPI2
  /* Initialize XSPI2, register MTD partitions, then re-enter memory-mapped
   * mode for NPU weight access.  The XSPI driver auto-suspends mmap mode
   * during MTD operations and re-enters after each completes.
   */

  ret = stm32_xspi_setup();
  if (ret < 0)
    {
      ferr("ERROR: stm32_xspi_setup failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_CDCACM) && !defined(CONFIG_CDCACM_CONSOLE)
  /* Register CDC/ACM serial device as /dev/ttyACM0 */

  ret = cdcacm_initialize(0, NULL);
  if (ret < 0)
    {
      ferr("ERROR: cdcacm_initialize failed: %d\n", ret);
    }
#endif

  UNUSED(ret);
  return OK;
}
