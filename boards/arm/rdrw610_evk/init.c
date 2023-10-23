/*
 * Copyright 2022 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/init.h>

#include <fsl_clock.h>
#include "fsl_io_mux.h"

static int rdrw610_evk_init(void)
{
	return 0;
}

SYS_INIT(rdrw610_evk_init, PRE_KERNEL_1, CONFIG_BOARD_INIT_PRIORITY);
