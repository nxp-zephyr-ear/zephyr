/*
 * Copyright 2022 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Header for nxp_rw610 platform
 *
 * This header file is used to specify and describe board-level aspects for the
 * 'nxp_rw610' platform.
 */

#ifndef _SOC__H_
#define _SOC__H_

#ifndef _ASMLANGUAGE
#include <zephyr/sys/util.h>
#include <fsl_common.h>

/* Add include for DTS generated information */
#include <zephyr/devicetree.h>

#endif /* !_ASMLANGUAGE */

/* Workaround to handle macro variation in the SDK */
#ifndef INPUTMUX_PINTSEL_COUNT
#define INPUTMUX_PINTSEL_COUNT INPUTMUX_PINT_SEL_COUNT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SOC__H_ */
