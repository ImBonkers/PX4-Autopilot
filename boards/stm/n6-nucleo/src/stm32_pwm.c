/****************************************************************************
 * boards/arm/stm32n6/nucleo-n657x0-q/src/stm32_pwm.c
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

#include <nuttx/timers/pwm.h>

#include "stm32_pwm.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_pwm_setup
 *
 * Description:
 *   Initialize PWM and register the PWM device.
 *
 ****************************************************************************/

int stm32_pwm_setup(void)
{
  struct pwm_lowerhalf_s *pwm;
  int ret;

#ifdef CONFIG_STM32N6_TIM1_PWM
  pwm = stm32_pwminitialize(1);
  if (pwm == NULL)
    {
      pwmerr("ERROR: Failed to get TIM1 PWM interface\n");
      return -ENODEV;
    }

  ret = pwm_register("/dev/pwm0", pwm);
  if (ret < 0)
    {
      pwmerr("ERROR: pwm_register /dev/pwm0 failed: %d\n", ret);
      return ret;
    }
#endif

#ifdef CONFIG_STM32N6_TIM3_PWM
  pwm = stm32_pwminitialize(3);
  if (pwm == NULL)
    {
      pwmerr("ERROR: Failed to get TIM3 PWM interface\n");
      return -ENODEV;
    }

  ret = pwm_register("/dev/pwm1", pwm);
  if (ret < 0)
    {
      pwmerr("ERROR: pwm_register /dev/pwm1 failed: %d\n", ret);
      return ret;
    }
#endif

  UNUSED(pwm);
  UNUSED(ret);
  return OK;
}
