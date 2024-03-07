# Copyright 2024 NXP
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nxp_el2go_claimcode_encryption)

# romapi source files
target_sources(app 
    PRIVATE 
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/iap/fsl_romapi_iap.c
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/flexspi/fsl_romapi_flexspi.c
)

#silicon_id source files
target_sources(app 
    PRIVATE 
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/components/silicon_id/fsl_silicon_id.c
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/components/silicon_id/socs/rw610/fsl_silicon_id_soc.c
)

#romapi header files
target_include_directories(app
    PRIVATE
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/iap
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/flexspi
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/nboot
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/devices/RW612/drivers/romapi/bootloader
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/boards/rdrw612bga/flash_config
)

#silicon_id header files
target_include_directories(app
    PRIVATE
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/components/silicon_id
        ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk/components/silicon_id/socs
)

zephyr_include_directories(${APPLICATION_SOURCE_DIR}/src)