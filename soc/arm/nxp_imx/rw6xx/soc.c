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
#include <fsl_power.h>
#include <fsl_clock.h>
#include <fsl_common.h>
#include <fsl_device_registers.h>
#include "soc.h"
#include "flexspi_clock_setup.h"
#include "fsl_ocotp.h"
#include "mcuxClEls.h"

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
	z_arm_reset,                                         /* 0x04 */
	z_arm_nmi,                                           /* 0x08 */
	z_arm_hard_fault,                                    /* 0x0C */
	z_arm_mpu_fault,                                     /* 0x10 */
	z_arm_bus_fault,                                     /* 0x14 */
	z_arm_usage_fault,                                   /* 0x18 */
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	z_arm_secure_fault, /* 0x1C */
#else
	z_arm_exc_spurious,
#endif                                  /* CONFIG_ARM_SECURE_FIRMWARE */
	(void (*)())_flash_used,        /* 0x20, imageLength. */
	0,                              /* 0x24, imageType (Plain Image) */
	0,                              /* 0x28, authBlockOffset/crcChecksum */
	z_arm_svc,                      /* 0x2C */
	z_arm_debug_monitor,            /* 0x30 */
	(void (*)())image_vector_table, /* 0x34, imageLoadAddress. */
	z_arm_pendsv,                   /* 0x38 */
#if defined(CONFIG_SYS_CLOCK_EXISTS) && defined(CONFIG_CORTEX_M_SYSTICK_INSTALL_ISR)
	sys_clock_isr, /* 0x3C */
#else
	z_arm_exc_spurious,
#endif
};
#endif /* CONFIG_NXP_RW6XX_BOOT_HEADER */

const clock_avpll_config_t avpll_config = {
	.ch1Freq = kCLOCK_AvPllChFreq12p288m,
	.ch2Freq = kCLOCK_AvPllChFreq64m,
	.enableCali = true
};

struct otp_gdet_data {
	uint32_t CFG0;
	uint32_t CFG1;
	uint32_t CFG2;
	uint32_t CFG3;
	uint32_t CFG4;
	uint32_t CFG5;
	uint32_t TRIM0;
};

static void load_gdet_cfg(struct otp_gdet_data *data, uint32_t pack)
{
	data->CFG3 = POWER_TrimSvc(data->CFG3, pack);

	/* GDET clock has been characterzed to 64MHz */
	CLKCTL0->ELS_GDET_CLK_SEL = CLKCTL0_ELS_GDET_CLK_SEL_SEL(2);

	/* Clear the GDET reset */
	RSTCTL0->PRSTCTL1_CLR = RSTCTL0_PRSTCTL1_CLR_ELS_GDET_REF_RST_N_MASK;

	/* Enable ELS */
	MCUX_CSSL_FP_FUNCTION_CALL_BEGIN(result, token, mcuxClEls_Enable_Async());
	if ((token != MCUX_CSSL_FP_FUNCTION_CALLED(mcuxClEls_Enable_Async)) ||
	    (result != MCUXCLELS_STATUS_OK_WAIT)) {
		assert(false);
	}
	MCUX_CSSL_FP_FUNCTION_CALL_END();

	/* Wait for the mcuxClEls_Enable_Async operation to complete. */
	MCUX_CSSL_FP_FUNCTION_CALL_BEGIN(result, token,
				mcuxClEls_WaitForOperation(MCUXCLELS_ERROR_FLAGS_CLEAR));
	/* mcuxClEls_WaitForOperation is a flow-protected function:
	 * Check the protection token and the return value
	 */
	if ((token != MCUX_CSSL_FP_FUNCTION_CALLED(mcuxClEls_WaitForOperation)) ||
	    (result != MCUXCLELS_STATUS_OK)) {
		assert(false);
	}
	MCUX_CSSL_FP_FUNCTION_CALL_END();

	/* LOAD command */
	MCUX_CSSL_FP_FUNCTION_CALL_BEGIN(result, token,
				mcuxClEls_GlitchDetector_LoadConfig_Async((uint8_t *)data));
	if ((token != MCUX_CSSL_FP_FUNCTION_CALLED(mcuxClEls_GlitchDetector_LoadConfig_Async)) ||
	    (result != MCUXCLELS_STATUS_OK_WAIT)) {
		assert(false);
	}
	MCUX_CSSL_FP_FUNCTION_CALL_END();
	/* Wait for the mcuxClEls_GlitchDetector_LoadConfig_Async
	 * operation to complete.
	 */
	MCUX_CSSL_FP_FUNCTION_CALL_BEGIN(result, token,
					 mcuxClEls_WaitForOperation(MCUXCLELS_ERROR_FLAGS_CLEAR));
	if ((token != MCUX_CSSL_FP_FUNCTION_CALLED(mcuxClEls_WaitForOperation)) ||
	    (result != MCUXCLELS_STATUS_OK)) {
		assert(false);
	}
	MCUX_CSSL_FP_FUNCTION_CALL_END();

	/* Wait for ELS ready */
	while ((ELS->ELS_STATUS & ELS_ELS_STATUS_ELS_BUSY_MASK) != 0U) {
	}
}

/* Configure voltage sensor and glitch detect blocks.
 * The configuration values are read from the OCOTP block
 */
static void config_svc_sensor(void)
{
	uint64_t svc;
	uint32_t pack;
	status_t status;
	struct otp_gdet_data gdetData;

	OCOTP_OtpInit();

	status = OCOTP_ReadSVC(&svc);

	if (status == kStatus_Success) {
		/* CES */
		status = OCOTP_ReadPackage(&pack);
		if (status == kStatus_Success) {
			/*
			 * A2 CES: Use SVC voltage.
			 * A1 CES: Keep boot voltage 1.11V.
			 */
			POWER_InitVoltage((uint32_t)svc >> 16, pack);
		}

		/* SVC GDET config */
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(149, &gdetData.CFG0) :
			  status;
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(150, &gdetData.CFG1) :
			  status;
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(151, &gdetData.CFG2) :
			  status;
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(155, &gdetData.CFG3) :
			  status;
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(153, &gdetData.CFG4) :
			  status;
		status = (status == kStatus_Success) ? OCOTP_OtpFuseRead(154, &gdetData.CFG5) :
			  status;

		if (status == kStatus_Success) {
			load_gdet_cfg(&gdetData, pack);
		}
	} else {
		/* A1/A2 non-CES */
		SystemCoreClockUpdate();

		/* LPBG trim */
		BUCK11->BUCK_CTRL_EIGHTEEN_REG = 0x6U;
		/* Change buck level */
		PMU->PMIP_BUCK_LVL = PMU_PMIP_BUCK_LVL_SLEEP_BUCK18_SEL(0x60U) |
			PMU_PMIP_BUCK_LVL_SLEEP_BUCK11_SEL(0x22U) |
			PMU_PMIP_BUCK_LVL_NORMAL_BUCK18_SEL(0x60U) |
			PMU_PMIP_BUCK_LVL_NORMAL_BUCK11_SEL(0x54U);
		/* Delay 600us */
		SDK_DelayAtLeastUs(600, SystemCoreClock);
	}

	OCOTP_OtpDeinit();
}

/**
 * @brief Initialize the system clocks and peripheral clocks
 */
static ALWAYS_INLINE void clock_init(void)
{
	POWER_DisableGDetVSensors();

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

	/* Do not configure the SVC sensor for A0 parts */
	if ((SOCCTRL->CHIP_INFO & SOCCIU_CHIP_INFO_REV_NUM_MASK) != 0U) {
		config_svc_sensor();
	}
	/* Move FLEXSPI clock source to T3 256m / 4 to avoid instruction/data fetch issue in XIP
	 * when updating PLL and main clock.
	 */
	set_flexspi_clock(FLEXSPI, 6U, 4U);

	/* First let M33 run on SOSC */
	CLOCK_AttachClk(kSYSOSC_to_MAIN_CLK);
	CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 1);

	/* tcpu_mci_clk configured to 260MHz, tcpu_mci_flexspi_clk 312MHz. */
	CLOCK_InitTcpuRefClk(3120000000UL, kCLOCK_TcpuFlexspiDiv10);
	/* Enable tcpu_mci_clk 260MHz. Keep tcpu_mci_flexspi_clk gated. */
	CLOCK_EnableClock(kCLOCK_TcpuMciClk);

	/* tddr_mci_flexspi_clk 320MHz */
	CLOCK_InitTddrRefClk(kCLOCK_TddrFlexspiDiv10);
	CLOCK_EnableClock(kCLOCK_TddrMciFlexspiClk); /* 320MHz */

	/* Enable AUX0 PLL to 260 MHz */
	CLOCK_SetClkDiv(kCLOCK_DivAux0PllClk, 1U);

	/* Init AVPLL and enable both channels */
	CLOCK_InitAvPll(&avpll_config);
	CLOCK_SetClkDiv(kCLOCK_DivAudioPllClk, 1U);

	/* Configure MainPll to 260MHz, then let CM33 run on Main PLL. */
	CLOCK_SetClkDiv(kCLOCK_DivSysCpuAhbClk, 1U);
	CLOCK_SetClkDiv(kCLOCK_DivMainPllClk, 1U);
	CLOCK_AttachClk(kMAIN_PLL_to_MAIN_CLK);

	/* Set SYSTICKFCLKDIV divider to value 1 */
	CLOCK_SetClkDiv(kCLOCK_DivSystickClk, 1U);
	CLOCK_AttachClk(kSYSTICK_DIV_to_SYSTICK_CLK);

	/* Set PLL FRG clock to 20MHz. */
	CLOCK_SetClkDiv(kCLOCK_DivPllFrgClk, 13U);

	/* Call function set_flexspi_clock() to set flexspi clock source to aux0_pll_clk in XIP. */
	set_flexspi_clock(FLEXSPI, 2U, 2U);

#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(os_timer), nxp_os_timer, okay)
	CLOCK_AttachClk(kLPOSC_to_OSTIMER_CLK);
#endif

#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(wwdt), nxp_lpc_wwdt, okay))
	CLOCK_AttachClk(kLPOSC_to_WDT0_CLK);
#else
	/* Allowed to select none if not being used for watchdog to
	 * reduce power
	 */
	CLOCK_AttachClk(kNONE_to_WDT0_CLK);
#endif
#if defined(CONFIG_ADC_MCUX_GAU) || defined(CONFIG_NXP_GAU_ACOMP) || defined(CONFIG_DAC_MCUX_GAU)
	/* Attack clock for GAU and reset */
	CLOCK_AttachClk(kMAIN_CLK_to_GAU_CLK);
	CLOCK_SetClkDiv(kCLOCK_DivGauClk, 1U);
	CLOCK_EnableClock(kCLOCK_Gau);
	RESET_PeripheralReset(kGAU_RST_SHIFT_RSTn);
#endif /* GAU */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(usb_otg), okay) && CONFIG_USB_DC_NXP_EHCI
	/* Enable system xtal from Analog */
	SYSCTL2->ANA_GRP_CTRL |= (1UL << SYSCTL2_ANA_GRP_CTRL_PU_AG_SHIFT);
	/* reset USB */
	RESET_PeripheralReset(kUSB_RST_SHIFT_RSTn);
	/* enable usb clock */
	CLOCK_EnableClock(kCLOCK_Usb);
	/* enable usb phy clock */
	CLOCK_EnableUsbhsPhyClock();
#endif

/* Any flexcomm can be USART */
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm0), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&(const clock_frg_clk_config_t){0, kCLOCK_FrgPllDiv, 255, 0});
	CLOCK_AttachClk(kFRG_to_FLEXCOMM0);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm1), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&(const clock_frg_clk_config_t){1, kCLOCK_FrgPllDiv, 255, 0});
	CLOCK_AttachClk(kFRG_to_FLEXCOMM1);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm2), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&(const clock_frg_clk_config_t){2, kCLOCK_FrgPllDiv, 255, 0});
	CLOCK_AttachClk(kFRG_to_FLEXCOMM2);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm3), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&(const clock_frg_clk_config_t){3, kCLOCK_FrgPllDiv, 255, 0});
	CLOCK_AttachClk(kFRG_to_FLEXCOMM3);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm14), nxp_lpc_usart, okay)) && CONFIG_SERIAL
	CLOCK_SetFRGClock(&(const clock_frg_clk_config_t){14, kCLOCK_FrgPllDiv, 255, 0});
	CLOCK_AttachClk(kFRG_to_FLEXCOMM14);
#endif

/* Any flexcomm can be I2C */
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm0), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM0);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm1), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM1);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm2), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM2);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm3), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM3);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm14), nxp_lpc_i2c, okay)) && CONFIG_I2C
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM14);
#endif
#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(rtc), nxp_lpc_rtc, okay)
	CLOCK_EnableXtal32K(true);
	CLOCK_AttachClk(kXTAL32K_to_CLK32K);
#endif

/* Clock flexcomms when used as SPI */
#ifdef CONFIG_SPI
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm0), nxp_lpc_spi, okay))
	/* Set up Flexcomm0 FRG to clock at 260 MHz from main clock */
	const clock_frg_clk_config_t flexcomm0_frg = {0, kCLOCK_FrgMainClk, 255, 0};

	CLOCK_SetFRGClock(&flexcomm0_frg);
	CLOCK_AttachClk(kFRG_to_FLEXCOMM0);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm1), nxp_lpc_spi, okay))
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM1);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm2), nxp_lpc_spi, okay))
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM2);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm3), nxp_lpc_spi, okay))
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM3);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm14), nxp_lpc_spi, okay))
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM14);
#endif
#endif /* CONFIG_SPI */

/* I2S flexcomm */
#ifdef CONFIG_I2S
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm0), nxp_lpc_i2s, okay))
	CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM0);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm1), nxp_lpc_i2s, okay))
	CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM1);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm2), nxp_lpc_i2s, okay))
	CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM2);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm3), nxp_lpc_i2s, okay))
	CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM3);
#endif
#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(flexcomm14), nxp_lpc_i2s, okay))
	CLOCK_AttachClk(kAUDIO_PLL_to_FLEXCOMM14);
#endif
#endif /* CONFIG_I2S */

#ifdef CONFIG_NET_L2_ETHERNET
	RESET_PeripheralReset(kENET_IPG_RST_SHIFT_RSTn);
	RESET_PeripheralReset(kENET_IPG_S_RST_SHIFT_RSTn);
	CLOCK_EnableClock(kCLOCK_TddrMciEnetClk);
#endif /* CONFIG_NET_L2_ETHERNET */

	/* Leave this disabled for A0 parts */
	if ((SOCCTRL->CHIP_INFO & SOCCIU_CHIP_INFO_REV_NUM_MASK) != 0U) {
		POWER_EnableGDetVSensors();
	}
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

#if (DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(wwdt), nxp_lpc_wwdt, okay))
	POWER_EnableResetSource(kPOWER_ResetSourceWdt);
#endif

	/* Initialize clock */
	clock_init();

#if defined(CONFIG_ADC_MCUX_GAU) || defined(CONFIG_NXP_GAU_ACOMP) || defined(CONFIG_DAC_MCUX_GAU)
	POWER_PowerOnGau();
#endif

	return 0;
}

void z_arm_platform_init(void)
{
	/* This is provided by the SDK */
	SystemInit();
}

SYS_INIT(nxp_rw600_init, PRE_KERNEL_1, 0);
