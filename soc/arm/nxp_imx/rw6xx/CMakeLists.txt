# Copyright 2022 NXP
#
# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  soc.c
  flexspi_clock_setup.c
  )

zephyr_sources_ifdef(CONFIG_PM
  power.c
  )

zephyr_library_include_directories(
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/${ARCH}/include
  )

zephyr_linker_sources_ifdef(CONFIG_NXP_RW6XX_BOOT_HEADER
  ROM_START SORT_KEY 0 boot_header.ld)

set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld CACHE INTERNAL "")

zephyr_code_relocate(FILES flexspi_clock_setup.c LOCATION RAM)
