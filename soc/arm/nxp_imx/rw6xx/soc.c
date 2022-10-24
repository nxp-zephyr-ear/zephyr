/*
 * Copyright 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for nxp rw6xx platform
 *
 * This module provides routines to initialize and support board-level
 * hardware for the nxp rw6xx platform.
 */

#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>

#include <cortex_m/exception.h>
#include <fsl_clock.h>
#include <fsl_common.h>
#include <fsl_device_registers.h>
#include <soc.h>

#ifdef CONFIG_NXP_RW6XX_BOOT_HEADER
extern char z_main_stack[];
extern char _flash_used[];

extern void z_arm_reset(void);
extern void z_arm_nmi(void);
extern void z_arm_hard_fault(void);
extern void z_arm_mpu_fault(void);
extern void z_arm_bus_fault(void);
extern void z_arm_usage_fault(void);
extern void z_arm_secure_fault(void);
extern void z_arm_svc(void);
extern void z_arm_debug_monitor(void);
extern void z_arm_pendsv(void);
extern void sys_clock_isr(void);
extern void z_arm_exc_spurious(void);

__imx_boot_ivt_section void (*const image_vector_table[])(void) = {
	(void (*)())(z_main_stack + CONFIG_MAIN_STACK_SIZE), /* 0x00 */
	z_arm_reset,					     /* 0x04 */
	z_arm_nmi,					     /* 0x08 */
	z_arm_hard_fault,				     /* 0x0C */
	z_arm_mpu_fault,				     /* 0x10 */
	z_arm_bus_fault,				     /* 0x14 */
	z_arm_usage_fault,				     /* 0x18 */
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	z_arm_secure_fault, /* 0x1C */
#else
	z_arm_exc_spurious,
#endif					/* CONFIG_ARM_SECURE_FIRMWARE */
	(void (*)())_flash_used,	/* 0x20, imageLength. */
	0,				/* 0x24, imageType (Plain Image) */
	0,				/* 0x28, authBlockOffset/crcChecksum */
	z_arm_svc,			/* 0x2C */
	z_arm_debug_monitor,		/* 0x30 */
	(void (*)())image_vector_table, /* 0x34, imageLoadAddress. */
	z_arm_pendsv,			/* 0x38 */
#if defined(CONFIG_SYS_CLOCK_EXISTS) && defined(CONFIG_CORTEX_M_SYSTICK_INSTALL_ISR)
	sys_clock_isr, /* 0x3C */
#else
	z_arm_exc_spurious,
#endif
};
#endif /* CONFIG_NXP_RW6XX_BOOT_HEADER */

const clock_avpll_config_t g_avpllConfig_BOARD_BootClockRUN = {
	.ch1Freq = kCLOCK_AvPllChFreq12p288m, .ch2Freq = kCLOCK_AvPllChFreq64m, .enableCali = true};

/**
 * @brief Initialize the system clock
 */
static ALWAYS_INLINE void clock_init(void)
{
#ifdef CONFIG_SOC_RW610
	if ((PMU->CAU_SLP_CTRL & PMU_CAU_SLP_CTRL_SOC_SLP_RDY_MASK) == 0U) {
		/* LPOSC not enabled, enable it */
		CLOCK_EnableClock(kCLOCK_RefClkCauSlp);
	}
	if ((SYSCTL2->SOURCE_CLK_GATE & SYSCTL2_SOURCE_CLK_GATE_REFCLK_SYS_CG_MASK) != 0U) {
		/* REFCLK_SYS not enabled, enable it */
		CLOCK_EnableClock(kCLOCK_RefClkSys);
	}

	/* Initialize T3 clocks and t3pll_mci_48_60m_irc configured to 48.3MHz */
	CLOCK_InitT3RefClk(kCLOCK_T3MciIrc48m);
	/* Enable FFRO */
	CLOCK_EnableClock(kCLOCK_T3PllMciIrcClk);
	/* Enable T3 256M clock and SFRO */
	CLOCK_EnableClock(kCLOCK_T3PllMci256mClk);

	/* First let M33 run on SOSC */
	CLOCK_AttachClk(kSYSOSC_to_MAIN_CLK);
	CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 1);

	/* Configure MainPll to 260MHz, then let CM33 run on Main PLL. */
	CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 1U);
	CLOCK_SetClkDiv(kCLOCK_DivMainPllClk, 1U);
	CLOCK_AttachClk(kMAIN_PLL_to_MAIN_CLK);

	/* Set SYSTICKFCLKDIV divider to value 1 */
	CLOCK_SetClkDiv(kCLOCK_DivSystickClk, 1U);
	CLOCK_AttachClk(kSYSTICK_DIV_to_SYSTICK_CLK);

	/* Set PLL FRG clock to 20MHz. */
	CLOCK_SetClkDiv(kCLOCK_DivPllFrgClk, 13U);

#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(wwdt), nxp_lpc_wwdt, okay))
	CLOCK_AttachClk(kLPOSC_to_WDT0_CLK);
#else
	/* Allowed to select none if not being used for watchdog to
	 * reduce power
	 */
	CLOCK_AttachClk(kNONE_to_WDT0_CLK);
#endif

#endif /* CONFIG_SOC_RW610 */
}

/**
 *
 * @brief Perform basic hardware initialization
 *
 * Initialize the interrupt controller device drivers.
 * Also initialize the timer device driver, if required.
 *
 * @return 0
 */

static int nxp_rw600_init(void)
{

	/* Initialize clock */
	clock_init();

	return 0;
}

SYS_INIT(nxp_rw600_init, PRE_KERNEL_1, 0);
