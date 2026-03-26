/****************************************************************************
 * boards/arm/stm32n6/nucleo-n657x0-q/src/stm32_npu.c
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

#ifdef CONFIG_STM32N6_NPU

#include <string.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/cache.h>
#include <nuttx/aie/ai_engine.h>

#include "arm_internal.h"
#include "stm32_rcc.h"
#include "stm32_npu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* STRENG register offsets */

#define STRENG_CTRL       0x00
#define STRENG_ADDR       0x08
#define STRENG_FSIZE      0x0c
#define STRENG_STRD       0x14
#define STRENG_FOFFSET    0x18
#define STRENG_FRAME_RPT  0x1c
#define STRENG_FRPTOFF    0x20
#define STRENG_POS        0x24
#define STRENG_LIMITEN    0x30
#define STRENG_LIMIT      0x34
#define STRENG_IRQ        0x3c
#define STRENG_CID_CACHE  0x48

#define STRSWITCH_CTRL    0x00
#define STRSWITCH_DST(n)  (0x08 + 4 * (n))

#define STRENG_CTRL_EN        (1 << 0)
#define STRENG_CTRL_CLR       (1 << 1)
#define STRENG_CTRL_SINGLE    (1 << 2)
#define STRENG_CTRL_DIR       (1 << 3)
#define STRENG_CTRL_RAW       (1 << 8)
#define STRENG_CTRL_RUNNING   (1u << 31)
#define STRENG_CTRL_SIZE_8BIT (8 << 16)

#define STRSWITCH_CTRL_EN       (1 << 0)
#define STRSWITCH_CTRL_CLR      (1 << 1)
#define STRSWITCH_CTRL_CONFCLR  (1 << 30)

#define NPU_SRC_ADDR      0x34100000
#define NPU_DST_ADDR      0x34100100
#define NPU_MCPY_BYTES    96
#define NPU_MCPY_ELEMENTS 96

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct
{
  uint32_t off;
  uint8_t  cnt;
} g_npu_blocks[] =
{
  { 0x00000,  1 },   /* CLKCTRL */
  { 0x01000,  1 },   /* INTCTRL */
  { 0x02000,  2 },   /* BUSIF */
  { 0x04000,  1 },   /* STRSWITCH */
  { 0x05000, 10 },   /* STRENG */
  { 0x0f000,  4 },   /* CONVACC */
  { 0x13000,  2 },   /* DECUN */
  { 0x15000,  2 },   /* ACTIV */
  { 0x17000,  4 },   /* ARITH */
  { 0x1b000,  2 },   /* POOL */
  { 0x1d000,  1 },   /* RECBUF */
  { 0x1e000,  1 },   /* EPOCHCTRL */
  { 0x1f000,  1 },   /* DBG_TRACE */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_npu_hw_init
 *
 * Description:
 *   NPU hardware init: RAMCFG, RIFSC, ATON fabric, CACHEAXI, STRENG test.
 *   This MUST be compiled in the board source (without LL_ATON headers)
 *   because the LL_ATON compat shim redefines ARM intrinsics that break
 *   register-level init code.
 *
 ****************************************************************************/

static int stm32_npu_hw_init(void)
{
  const int nblocks = sizeof(g_npu_blocks) / sizeof(g_npu_blocks[0]);
  int pass = 0;
  int total = 0;
  int i;
  int j;
  uint32_t t;

  putreg32(RCC_AHB2ENR_RAMCFGEN, STM32_RCC_AHB2ENSR);

  modifyreg32(STM32_RAMCFG_BASE + 0x080, (1 << 20), 0);  /* SRAM2 */
  modifyreg32(STM32_RAMCFG_BASE + 0x100, (1 << 20), 0);  /* SRAM3 */
  modifyreg32(STM32_RAMCFG_BASE + 0x180, (1 << 20), 0);  /* SRAM4 */
  modifyreg32(STM32_RAMCFG_BASE + 0x200, (1 << 20), 0);  /* SRAM5 */
  modifyreg32(STM32_RAMCFG_BASE + 0x280, (1 << 20), 0);  /* SRAM6 */

  putreg32(0x310, STM32_RIFSC_BASE + 0xc14);
  modifyreg32(STM32_RIFSC_BASE + 0x1c, 0, (1 << 10));
  modifyreg32(STM32_RIFSC_BASE + 0x3c, 0, (1 << 10));

  t = getreg32(STM32_NPU_BASE + 0x0);
  t |= (1 << 1);
  putreg32(t, STM32_NPU_BASE + 0x0);

  putreg32(1, STM32_NPU_BASE + 0x00);
  putreg32(0xffffffff, STM32_NPU_BASE + 0x08);
  putreg32(0xffffffff, STM32_NPU_BASE + 0x0c);
  putreg32(0xffffffff, STM32_NPU_BASE + 0x10);

  putreg32(1, STM32_NPU_BASE + 0x2000);
  putreg32(1, STM32_NPU_BASE + 0x3000);
  putreg32(1, STM32_NPU_BASE + 0x1000);

  modifyreg32(STM32_SYSCFG_BASE + 0x78, 1, 0);

  putreg32((1 << 10), STM32_RCC_MEMENSR);
  putreg32((1u << 30), STM32_RCC_AHB5RSTSR);
  {
    volatile int d;
    for (d = 0; d < 1000; d++);
  }

  putreg32((1u << 30), STM32_RCC_AHB5RSTCR);
  putreg32(0x02, STM32_CACHEAXI_BASE + 0x00);
  while (getreg32(STM32_CACHEAXI_BASE + 0x04) & 1);
  putreg32(0x01 | 0x3f0f0000, STM32_CACHEAXI_BASE + 0x00);

  for (i = 0; i < nblocks; i++)
    {
      for (j = 0; j < g_npu_blocks[i].cnt; j++)
        {
          uint32_t addr = STM32_NPU_BASE + g_npu_blocks[i].off
                          + 0x1000 * j + 0x4;
          total++;
          if (getreg32(addr) != 0)
            {
              pass++;
            }
        }
    }

  syslog(LOG_DEBUG, "NPU: %d/%d blocks alive\n", pass, total);

  /* NOTE: No blanket bus/memory clock enables here.  Enabling ACLKN +
   * any MEMENR bits together breaks XSPI2 indirect-mode reads (returns
   * all zeros).  SRAM clocks are already enabled by stm32_start.c.
   */

  /* STRENG memcopy self-test */

  {
    uint32_t se0 = STM32_NPU_BASE + 0x5000;
    uint32_t se1 = STM32_NPU_BASE + 0x5000 + 0x1000;
    uint32_t strswitch = STM32_NPU_BASE + 0x4000;
    uint32_t ctrl;
    int timeout;
    uint8_t *src = (uint8_t *)NPU_SRC_ADDR;
    uint8_t *dst = (uint8_t *)NPU_DST_ADDR;

    for (i = 0; i < NPU_MCPY_BYTES; i++)
      {
        src[i] = (uint8_t)i;
      }

    memset(dst, 0xaa, NPU_MCPY_BYTES);
    up_clean_dcache((uintptr_t)src, (uintptr_t)src + NPU_MCPY_BYTES);
    up_clean_dcache((uintptr_t)dst, (uintptr_t)dst + NPU_MCPY_BYTES);
    up_invalidate_dcache((uintptr_t)dst, (uintptr_t)dst + NPU_MCPY_BYTES);

    putreg32(STRSWITCH_CTRL_CLR, strswitch + STRSWITCH_CTRL);
    timeout = 10000;
    while ((getreg32(strswitch + STRSWITCH_CTRL) & STRSWITCH_CTRL_CLR)
           && --timeout > 0);
    putreg32(STRSWITCH_CTRL_CONFCLR, strswitch + STRSWITCH_CTRL);
    timeout = 10000;
    while ((getreg32(strswitch + STRSWITCH_CTRL) & STRSWITCH_CTRL_CONFCLR)
           && --timeout > 0);

    putreg32(STRENG_CTRL_CLR, se0 + STRENG_CTRL);
    timeout = 10000;
    while ((getreg32(se0 + STRENG_CTRL) & STRENG_CTRL_CLR) && --timeout > 0);
    putreg32(0xff, se0 + STRENG_IRQ);
    putreg32((uintptr_t)src, se0 + STRENG_ADDR);
    putreg32(NPU_MCPY_ELEMENTS, se0 + STRENG_FSIZE);
    putreg32(0, se0 + STRENG_STRD);
    putreg32(NPU_MCPY_BYTES, se0 + STRENG_FOFFSET);
    putreg32(0, se0 + STRENG_FRAME_RPT);
    putreg32(0, se0 + STRENG_FRPTOFF);
    putreg32(0x00000024, se0 + STRENG_POS);
    putreg32(0x04, se0 + STRENG_LIMITEN);
    putreg32(1, se0 + STRENG_LIMIT);
    putreg32(0x01, se0 + STRENG_CID_CACHE);
    ctrl = STRENG_CTRL_SINGLE | STRENG_CTRL_RAW | STRENG_CTRL_SIZE_8BIT;
    putreg32(ctrl, se0 + STRENG_CTRL);

    putreg32(STRENG_CTRL_CLR, se1 + STRENG_CTRL);
    timeout = 10000;
    while ((getreg32(se1 + STRENG_CTRL) & STRENG_CTRL_CLR) && --timeout > 0);
    putreg32(0xff, se1 + STRENG_IRQ);
    putreg32((uintptr_t)dst, se1 + STRENG_ADDR);
    putreg32(NPU_MCPY_ELEMENTS, se1 + STRENG_FSIZE);
    putreg32(0, se1 + STRENG_STRD);
    putreg32(NPU_MCPY_BYTES, se1 + STRENG_FOFFSET);
    putreg32(0, se1 + STRENG_FRAME_RPT);
    putreg32(0, se1 + STRENG_FRPTOFF);
    putreg32(0x00000024, se1 + STRENG_POS);
    putreg32(0x04, se1 + STRENG_LIMITEN);
    putreg32(1, se1 + STRENG_LIMIT);
    putreg32(0x01, se1 + STRENG_CID_CACHE);
    ctrl = STRENG_CTRL_SINGLE | STRENG_CTRL_DIR | STRENG_CTRL_RAW
           | STRENG_CTRL_SIZE_8BIT;
    putreg32(ctrl, se1 + STRENG_CTRL);

    putreg32(STRSWITCH_CTRL_EN, strswitch + STRSWITCH_CTRL);
    putreg32(0x01, strswitch + STRSWITCH_DST(1));

    ctrl = getreg32(se1 + STRENG_CTRL);
    putreg32(ctrl | STRENG_CTRL_EN, se1 + STRENG_CTRL);
    ctrl = getreg32(se0 + STRENG_CTRL);
    putreg32(ctrl | STRENG_CTRL_EN, se0 + STRENG_CTRL);

    timeout = 1000000;
    while ((getreg32(se1 + STRENG_CTRL) & STRENG_CTRL_RUNNING)
           && --timeout > 0);

    up_invalidate_dcache((uintptr_t)dst, (uintptr_t)dst + NPU_MCPY_BYTES);

    if (timeout <= 0)
      {
        syslog(LOG_ERR, "NPU STRENG memcpy: TIMEOUT\n");
        return -EIO;
      }
    else if (memcmp(src, dst, NPU_MCPY_BYTES) != 0)
      {
        syslog(LOG_ERR, "NPU STRENG memcpy: MISMATCH\n");
        return -EIO;
      }
    else
      {
        syslog(LOG_DEBUG, "NPU STRENG memcpy: %d bytes PASS\n",
               NPU_MCPY_BYTES);
      }
  }

  /* Disable CACHEAXI controller after self-test.  The CACHEAXI bus clock
   * must stay on (HPDMA1 needs it for AXI access).  Only the controller
   * enable (CR1.EN) is cleared — the caching logic is inactive but the
   * clock fabric remains powered for DMA.
   *
   * CRITICAL: Do NOT write to NPU INTCTRL registers between this point
   * and XSPI2 init.  AXI traffic to the NPU fabric while the CACHEAXI
   * controller is freshly disabled can wake the caching logic and cause
   * XSPI2 indirect-mode reads to return stale zeros.  INTCTRL setup is
   * deferred to the first NPUIOC_RUN_SYNC call.
   */

  putreg32(0, STM32_CACHEAXI_BASE + 0x00);  /* CR1 = 0: disable controller */

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int stm32_npu_setup(void)
{
  FAR struct aie_lowerhalf_s *lower;
  int ret;

  /* HW init in board context (no LL_ATON headers) */

  ret = stm32_npu_hw_init();
  if (ret < 0)
    {
      ferr("ERROR: NPU HW init failed: %d\n", ret);
      return ret;
    }

  /* Driver init in chip context (has LL_ATON headers) */

  lower = stm32_npu_initialize();
  if (lower == NULL)
    {
      ferr("ERROR: stm32_npu_initialize failed\n");
      return -ENODEV;
    }

  ret = aie_register("/dev/npu0", lower);
  if (ret < 0)
    {
      ferr("ERROR: aie_register failed: %d\n", ret);
    }

  return ret;
}

#endif /* CONFIG_STM32N6_NPU */
