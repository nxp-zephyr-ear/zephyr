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
	const clock_frg_clk_config_t debug_clock_conf = {3, kCLOCK_FrgPllDiv, 255, 0};

#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm3), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&debug_clock_conf);
	CLOCK_AttachClk(kFRG_to_FLEXCOMM3);
#endif

#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm2), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM2);
#endif

	return 0;
}

SYS_INIT(rdrw610_evk_init, PRE_KERNEL_1, CONFIG_BOARD_INIT_PRIORITY);
