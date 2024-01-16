/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief Linker command/script file
 *
 * This is the linker script for both standard images and XIP images.
 */

#include <autoconf.h>

MEMORY
    {
    m_mbox1               (rw) : ORIGIN = 0x41380000, LENGTH = 0x00000488
    m_txq1                (rw) : ORIGIN = 0x41380488, LENGTH = 0x00001000
    m_mbox2               (rw) : ORIGIN = 0x443C0000, LENGTH = 0x00000488
    m_txq23               (rw) : ORIGIN = 0x443C0488, LENGTH = 0x00001080
    m_txq32               (rw) : ORIGIN = 0x443C1508, LENGTH = 0x00001080
    }

SECTIONS
    {
  .smu_cpu13_mbox (NOLOAD) :
  {
    . = ALIGN(4);
    *(.smu_cpu13_mbox)
    KEEP (*(.smu_cpu13_mbox))
    . = ALIGN(4);
  } > m_mbox1

  .smu_cpu31_txq (NOLOAD) :
  {
    . = ALIGN(4);
    *(.smu_cpu31_txq)
    KEEP (*(.smu_cpu31_txq))
    . = ALIGN(4);
  } > m_txq1

  .smu_cpu23_mbox (NOLOAD) :
  {
    . = ALIGN(4);
    *(.smu_cpu23_mbox)
    KEEP (*(.smu_cpu23_mbox))
    . = ALIGN(4);
  } > m_mbox2

  .smu_cpu32_txq (NOLOAD) :
  {
    . = ALIGN(4);
    *(.smu_cpu32_txq)
    KEEP (*(.smu_cpu32_txq))
    . = ALIGN(4);
  } > m_txq32
    }

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>
