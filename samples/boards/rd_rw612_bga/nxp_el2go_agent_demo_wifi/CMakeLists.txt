# Copyright 2024 NXP
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nxp_el2go_agent_demo_wifi)

target_sources(app
  PRIVATE
    ${APPLICATION_SOURCE_DIR}/src/psa_get_key_enrollment_algorithm.c
)

target_include_directories(app
  PRIVATE
    $<TARGET_PROPERTY:tfm,TFM_BINARY_DIR>/api_ns/interface/include
)

zephyr_include_directories(${APPLICATION_SOURCE_DIR}/src)
