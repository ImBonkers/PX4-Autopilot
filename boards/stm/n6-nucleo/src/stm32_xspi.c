/****************************************************************************
 * boards/arm/stm32n6/nucleo-n657x0-q/src/stm32_xspi.c
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

#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <debug.h>

#include <nuttx/mtd/mtd.h>
#include <nuttx/signal.h>
#include <nuttx/spi/qspi.h>

#include "stm32_xspi.h"

#ifdef CONFIG_STM32N6_XSPI

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MX25UM51245G status register bits */

#define MX25_SR_WIP           (1 << 0)
#define MX25_SR_WEL           (1 << 1)
#define MX25_SR_BP_MASK       (0x3c)  /* BP3:BP0 bits 5:2 */
#define MX25_SR_SRWD          (1 << 7)
#define MX25_SR_PROT_MASK     (MX25_SR_BP_MASK | MX25_SR_SRWD)

/* Standard SPI flash commands */

#define MX25_CMD_RDSR         0x05
#define MX25_CMD_WREN         0x06
#define MX25_CMD_WRSR         0x01

/* Flash partition layout (64MB MX25UM51245G)
 *
 * All sizes in 256-byte blocks (the MTD blocksize for this device).
 * Each erase block is 4KB = 16 blocks.
 *
 *  Region  | Flash Addr  | Block Offset | Blocks | Size
 *  --------|-------------|--------------|--------|------
 *  FSBL    | 0x70000000  | 0            | 512    | 128KB
 *  NuttX   | 0x70020000  | 512          | 4096   | 1MB
 *  NPU     | 0x70120000  | 4608         | 65536  | 16MB
 *  FS      | 0x71120000  | 70144        | rest   | ~47MB
 */

#define FLASH_FSBL_FIRSTBLK    0
#define FLASH_FSBL_NBLOCKS     512       /* 128KB / 256 */

#define FLASH_NUTTX_FIRSTBLK   512
#define FLASH_NUTTX_NBLOCKS    4096      /* 1MB / 256 */

#define FLASH_NPU_FIRSTBLK     4608
#define FLASH_NPU_NBLOCKS      65536     /* 16MB / 256 */

#define FLASH_FS_FIRSTBLK      70144

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mx25_flash_unprotect
 *
 * Description:
 *   Clear block protection bits in the MX25UM51245G status register.
 *   CubeProgrammer or FSBL may set BP bits, causing erase/write operations
 *   to be silently rejected.  The core mx25rxx driver does not clear BP
 *   bits for OPI-class flash.
 *
 ****************************************************************************/

static void mx25_flash_unprotect(struct qspi_dev_s *qspi)
{
  struct qspi_cmdinfo_s cmdinfo;
  uint8_t sr;

  /* Read status register */

  cmdinfo.flags   = QSPICMD_READDATA;
  cmdinfo.addrlen = 0;
  cmdinfo.cmd     = MX25_CMD_RDSR;
  cmdinfo.buflen  = 1;
  cmdinfo.addr    = 0;
  cmdinfo.buffer  = &sr;

  QSPI_LOCK(qspi, true);
  QSPI_COMMAND(qspi, &cmdinfo);

  if ((sr & MX25_SR_PROT_MASK) != 0)
    {
      /* Send WREN */

      cmdinfo.flags   = 0;
      cmdinfo.cmd     = MX25_CMD_WREN;
      cmdinfo.buflen  = 0;
      cmdinfo.buffer  = NULL;
      QSPI_COMMAND(qspi, &cmdinfo);

      /* Write SR = 0x00 to clear all BP bits and SRWD */

      sr = 0;
      cmdinfo.flags   = QSPICMD_WRITEDATA;
      cmdinfo.cmd     = MX25_CMD_WRSR;
      cmdinfo.buflen  = 1;
      cmdinfo.buffer  = &sr;
      QSPI_COMMAND(qspi, &cmdinfo);

      /* Wait for write to complete */

      do
        {
          nxsig_usleep(1000);
          cmdinfo.flags   = QSPICMD_READDATA;
          cmdinfo.cmd     = MX25_CMD_RDSR;
          cmdinfo.buflen  = 1;
          cmdinfo.buffer  = &sr;
          QSPI_COMMAND(qspi, &cmdinfo);
        }
      while ((sr & MX25_SR_WIP) != 0);

      finfo("XSPI: protection cleared, SR=0x%02x\n", sr);
    }

  QSPI_LOCK(qspi, false);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_xspi_setup
 *
 * Description:
 *   Initialize XSPI2 and register the MX25UM51245G as partitioned MTD
 *   devices.  The 64MB flash is split into four partitions:
 *
 *     /dev/mtdblock0 — FSBL   (128KB, read-only boot image)
 *     /dev/mtdblock1 — NuttX  (1MB, read-only application image)
 *     /dev/mtdblock2 — NPU    (16MB, DNN weight storage)
 *     /dev/mtdblock3 — FS     (~47MB, littlefs filesystem)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int stm32_xspi_setup(void)
{
  struct qspi_dev_s *qspi;
  struct mtd_dev_s *mtd;
#ifdef CONFIG_MTD_PARTITION
  struct mtd_dev_s *part;
  struct mtd_geometry_s geo;
  off_t totalblocks;
#endif
  int ret;

  /* Initialize the XSPI2 driver */

  qspi = stm32_xspi_initialize(0);
  if (qspi == NULL)
    {
      ferr("ERROR: stm32_xspi_initialize failed\n");
      return -ENODEV;
    }

  /* Initialize the MX25RXX MTD driver.
   *
   * NOTE: The mx25rxx driver supports MX25R/MX25L parts (JEDEC type
   * 0x28/0x20). The MX25UM51245G has JEDEC type 0x80 (Octal). In SPI
   * 1-1-1 mode, it responds to standard SPI commands but with a different
   * JEDEC ID. If mx25rxx_initialize fails, the flash is still accessible
   * via direct QSPI command/memory ops.
   */

  mtd = mx25rxx_initialize(qspi, true);
  if (mtd == NULL)
    {
      fwarn("WARNING: mx25rxx_initialize failed (expected for MX25UM)\n");
      return OK;
    }

  /* Clear block protection bits if set.  Must be done after
   * mx25rxx_initialize (which enters 4-byte address mode) but before
   * any erase/write operations.
   */

  mx25_flash_unprotect(qspi);

#ifdef CONFIG_MTD_PARTITION
  /* Query flash geometry to compute filesystem partition size */

  ret = MTD_IOCTL(mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
  if (ret < 0)
    {
      ferr("ERROR: MTD_IOCTL(GEOMETRY) failed: %d\n", ret);
      return ret;
    }

  totalblocks = (off_t)geo.neraseblocks * (geo.erasesize / geo.blocksize);
  finfo("Flash: %lu erase blocks, blocksize=%lu, erasesize=%lu, "
        "total blocks=%ld\n",
        (unsigned long)geo.neraseblocks, (unsigned long)geo.blocksize,
        (unsigned long)geo.erasesize, (long)totalblocks);

  /* Partition 0: FSBL (128KB) — /dev/mtdblock0 */

  part = mtd_partition(mtd, FLASH_FSBL_FIRSTBLK, FLASH_FSBL_NBLOCKS);
  if (part != NULL)
    {
      register_mtddriver("/dev/mtdblock0", part, 0444, NULL);
    }

  /* Partition 1: NuttX (1MB) — /dev/mtdblock1 */

  part = mtd_partition(mtd, FLASH_NUTTX_FIRSTBLK, FLASH_NUTTX_NBLOCKS);
  if (part != NULL)
    {
      register_mtddriver("/dev/mtdblock1", part, 0444, NULL);
    }

  /* Partition 2: NPU weights (16MB) — /dev/mtdblock2 */

  part = mtd_partition(mtd, FLASH_NPU_FIRSTBLK, FLASH_NPU_NBLOCKS);
  if (part != NULL)
    {
      register_mtddriver("/dev/mtdblock2", part, 0444, NULL);
    }

  /* Partition 3: Filesystem (~47MB) — /dev/mtdblock3 */

  part = mtd_partition(mtd, FLASH_FS_FIRSTBLK,
                       totalblocks - FLASH_FS_FIRSTBLK);
  if (part != NULL)
    {
      register_mtddriver("/dev/mtdblock3", part, 0755, NULL);
      finfo("XSPI2 flash partitioned: fsbl=128K nuttx=1M npu=16M fs=%ldK\n",
            (long)((totalblocks - FLASH_FS_FIRSTBLK) * geo.blocksize / 1024));
    }

#ifdef CONFIG_FS_LITTLEFS
  /* Mount littlefs on the filesystem partition only */

  mkdir("/fs", 0777);
  mkdir("/fs/microsd", 0777);
  ret = nx_mount("/dev/mtdblock3", "/fs/microsd", "littlefs", 0, NULL);
  if (ret < 0)
    {
      ret = nx_mount("/dev/mtdblock3", "/fs/microsd", "littlefs", 0,
                     "forceformat");
      if (ret < 0)
        {
          ferr("ERROR: littlefs format+mount failed: %d\n", ret);
        }
    }
#endif /* CONFIG_FS_LITTLEFS */

#else /* !CONFIG_MTD_PARTITION */
  /* No partition support — register the whole flash as a single device */

  ret = register_mtddriver("/dev/mtdblock0", mtd, 0755, NULL);
  if (ret < 0)
    {
      ferr("ERROR: register_mtddriver failed: %d\n", ret);
      return ret;
    }

  finfo("XSPI2 flash registered as /dev/mtdblock0 (unpartitioned)\n");

#ifdef CONFIG_FS_LITTLEFS
  mkdir("/mnt", 0777);
  ret = nx_mount("/dev/mtdblock0", "/mnt", "littlefs", 0, NULL);
  if (ret < 0)
    {
      fwarn("littlefs mount failed (%d), formatting...\n", ret);
      ret = nx_mount("/dev/mtdblock0", "/mnt", "littlefs", 0,
                     "forceformat");
      if (ret < 0)
        {
          ferr("ERROR: littlefs format+mount failed: %d\n", ret);
        }
    }
#endif /* CONFIG_FS_LITTLEFS */
#endif /* CONFIG_MTD_PARTITION */

  /* Enter memory-mapped mode for NPU weight access at 0x70000000+.
   * Uses SPI 1-1-1 Fast Read (0x0C) with 4-byte address and 8 dummy
   * cycles.  The XSPI driver auto-suspends mmap during MTD operations
   * (xspi_lock/unlock) and re-enters after each completes.
   */

  {
    struct qspi_meminfo_s mmap_meminfo;

    mmap_meminfo.flags   = QSPIMEM_READ;
    mmap_meminfo.addrlen = 4;
    mmap_meminfo.dummies = 8;
    mmap_meminfo.cmd     = 0x0c;
    mmap_meminfo.addr    = 0;
    mmap_meminfo.buflen  = 0;
    mmap_meminfo.buffer  = NULL;

    stm32_xspi_enter_memorymapped(qspi, &mmap_meminfo, 0);
    finfo("XSPI2 memory-mapped mode enabled (Fast Read 0x0C)\n");
  }

  return OK;
}

#endif /* CONFIG_STM32N6_XSPI */
