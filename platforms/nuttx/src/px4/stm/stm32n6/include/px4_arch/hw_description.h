/****************************************************************************
 * STM32N6 hardware description — stub for initial bringup
 *
 * STM32N6 uses GPDMA1/HPDMA1 (not DMA1/DMA2), so the stm32_common
 * DMA channel mapping does not apply. Timer DMA integration will be
 * implemented when PWM output support is added.
 ****************************************************************************/
#pragma once

#include <stdint.h>
