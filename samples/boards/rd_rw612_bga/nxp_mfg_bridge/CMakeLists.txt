#Copyright 2023 - 2024 NXP
#SPDX - License - Identifier : Apache - 2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nxp_mfg_bridge)

zephyr_include_directories(
  ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/components/rpmsg
  ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/components/conn_fwloader/include
  )

zephyr_library_sources(${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/utilities/misc_utilities/fsl_memcpy.S)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
