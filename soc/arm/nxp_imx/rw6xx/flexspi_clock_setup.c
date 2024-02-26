/*
 * Copyright 2022 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include "flexspi_clock_setup.h"

/**
 * @brief Safely initialize the flexspi for XIP
 */
void flexspi_safe_config(void)
{
	FLEXSPI_Type *base = (FLEXSPI_Type *) DT_REG_ADDR(DT_NODELABEL(flexspi));
	const uint32_t src = 6;
	const uint32_t divider = 2;
	uint32_t status;
	uint32_t lastStatus;
	uint32_t retry;

	/* If flexspi is not already configured */
	if ((CLKCTL0->FLEXSPIFCLKSEL != CLKCTL0_FLEXSPIFCLKSEL_SEL(src)) ||
	((CLKCTL0->FLEXSPIFCLKDIV & CLKCTL0_FLEXSPIFCLKDIV_DIV_MASK) != (divider - 1))) {
		/* Enable FLEXSPI clock */
		CLKCTL0->PSCCTL0_SET = CLKCTL0_PSCCTL0_SET_FLEXSPI0_MASK;
		/* Enable FLEXSPI */
		base->MCR0 &= ~FLEXSPI_MCR0_MDIS_MASK;
		/* Wait until FLEXSPI is not busy */
		while (!((base->STS0 & FLEXSPI_STS0_ARBIDLE_MASK) &&
			(base->STS0 & FLEXSPI_STS0_SEQIDLE_MASK)))
			;
		/* Disable module during reset procedure */
		base->MCR0 |= FLEXSPI_MCR0_MDIS_MASK;
		/* Disable clock before changing clock source */
		CLKCTL0->PSCCTL0_CLR = CLKCTL0_PSCCTL0_CLR_FLEXSPI0_MASK;
		/* Update FLEXSPI clock */
		CLKCTL0->FLEXSPIFCLKSEL = CLKCTL0_FLEXSPIFCLKSEL_SEL(src);
		CLKCTL0->FLEXSPIFCLKDIV |= CLKCTL0_FLEXSPIFCLKDIV_RESET_MASK;
		CLKCTL0->FLEXSPIFCLKDIV = CLKCTL0_FLEXSPIFCLKDIV_DIV(divider - 1);
		while ((CLKCTL0->FLEXSPIFCLKDIV) & CLKCTL0_FLEXSPIFCLKDIV_REQFLAG_MASK)
			;
		/* Enable FLEXSPI clock again */
		CLKCTL0->PSCCTL0_SET = CLKCTL0_PSCCTL0_SET_FLEXSPI0_MASK;
		/* Recommended setting */
		base->DLLCR[0] = 0x1U;
		/* Enable FLEXSPI module */
		base->MCR0 &= ~FLEXSPI_MCR0_MDIS_MASK;
		base->MCR0 |= FLEXSPI_MCR0_SWRESET_MASK;
		while (base->MCR0 & FLEXSPI_MCR0_SWRESET_MASK)
			;
		/* Wait until DLL locked if enabled */
		if (0U != (base->DLLCR[0] & FLEXSPI_DLLCR_DLLEN_MASK)) {
			lastStatus = base->STS2;
			retry = 10;
			/* Wait until slave delay line and slave reference delay line is locked. */
			do {
				status = base->STS2;
				if ((status & (FLEXSPI_STS2_AREFLOCK_MASK |
				     FLEXSPI_STS2_ASLVLOCK_MASK)) ==
				    (FLEXSPI_STS2_AREFLOCK_MASK |
				     FLEXSPI_STS2_ASLVLOCK_MASK)) {
					/* Locked */
					retry = 100;
					break;
				} else if (status == lastStatus) {
					/* Same delay cell number in calibration */
					retry--;
				} else {
					retry = 10;
					lastStatus = status;
				}
			} while (retry > 0);
		}
		/* Need to delay at least 100 NOPs to ensure the DLL is locked */
		for (; retry > 0; retry--)
			__NOP();
	}
}
