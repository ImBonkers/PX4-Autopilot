/****************************************************************************
 * boards/arm/stm32n6/nucleo-n657x0-q/src/stm32_adc.c
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

#include <errno.h>
#include <debug.h>

#include <nuttx/analog/adc.h>

#include <nuttx/arch.h>
#include <arm_internal.h>
#include "stm32_gpio.h"
#include "stm32_adc.h"
#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ADC channel-to-pin mapping on NUCLEO-N657X0-Q:
 *
 * The ADC operates on the 1.8V domain (VDDA18ADC).  Only pins on the
 * 1.8V I/O domain connect properly to the ADC analog inputs.  The
 * Arduino analog header pins (A0-A5) route through 3.3V domain pins
 * by default and need solder bridge changes for ADC use (UM3417 Table 12).
 *
 * Direct 1.8V ADC pins accessible on morpho connectors:
 *   PA1  -> ADC12_INP1   (CN15 pin 38) — used by ST ADC example
 *   PA8  -> ADC12_INP5   (Arduino A0 with SB mod)
 *   PA9  -> ADC12_INP10  (Arduino A1 with SB mod)
 *   PA10 -> ADC12_INP11  (Arduino A2 with SB mod)
 *
 * IMPORTANT: Input voltage must be <= 1.8V on these pins!
 */

#define GPIO_ADC1_IN1   (GPIO_ANALOG | GPIO_PORTA | GPIO_PIN1)

#define ADC1_NCHANNELS  1

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const uint8_t g_adc1_chanlist[ADC1_NCHANNELS] =
{
  1,    /* PA1 = ADC12_INP1 (morpho CN15 pin 38) */
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_adc_setup
 *
 * Description:
 *   Configure ADC GPIO pins and register /dev/adc0.
 *   Called from stm32_bringup().
 *
 ****************************************************************************/

int stm32_adc_setup(void)
{
  struct adc_dev_s *adc;
  int ret;

  /* Configure analog GPIO pin for external channel */

  stm32_configgpio(GPIO_ADC1_IN1);

  /* Configure security attributes for ADC analog path.
   *
   * On STM32N6 with TrustZone, both the ADC peripheral and the GPIO
   * pin must have matching Secure attributes for the analog path to
   * connect properly.
   *
   * 1) RIFSC: Mark ADC12 as Secure (RISC_SECCFGR2 bit 0)
   * 2) GPIO:  Mark PA1 as Secure (GPIOA SECCFGR bit 1)
   */

  /* RIFSC ADC12 Secure: RISC_SECCFGR2 at RIFSC_BASE + 0x018, bit 0 */

  modifyreg32(STM32_RIFSC_BASE + 0x018, 0, (1 << 0));

  /* GPIO PA1 Secure: GPIOA_SECCFGR at GPIOA_BASE + 0x30, bit 1 */

  modifyreg32(STM32_GPIOA_BASE + 0x30, 0, (1 << 1));

  /* Initialize ADC1 */

  adc = stm32n6_adc_initialize(1, g_adc1_chanlist, ADC1_NCHANNELS);
  if (adc == NULL)
    {
      aerr("ERROR: stm32n6_adc_initialize(1) failed\n");
      return -ENODEV;
    }

  /* Register as /dev/adc0 */

  ret = adc_register("/dev/adc0", adc);
  if (ret < 0)
    {
      aerr("ERROR: adc_register(/dev/adc0) failed: %d\n", ret);
      return ret;
    }

  ainfo("ADC1 registered at /dev/adc0\n");
  return OK;
}
