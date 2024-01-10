/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/init.h>

#include "fsl_power.h"

#include <cmsis_core.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/timer/system_timer.h>

LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

/* Active mode */
#define POWER_MODE0		0
/* Idle mode */
#define POWER_MODE1		1
/* Standby mode */
#define POWER_MODE2		2
/* Sleep mode */
#define POWER_MODE3		3
/* Deep Sleep mode */
#define POWER_MODE4		4

#define NODE_ID DT_INST(0, nxp_pdcfg_power)

extern void clock_init(void);

power_sleep_config_t slp_cfg;

#ifdef CONFIG_MPU

#define MPU_NODE_ID DT_INST(0, arm_armv8m_mpu)
#define MPU_NUM_REGIONS DT_PROP(MPU_NODE_ID, arm_num_mpu_regions)

typedef struct mpu_conf {
	uint32_t CTRL;
	uint32_t RNR;
	uint32_t RBAR[MPU_NUM_REGIONS];
	uint32_t RLAR[MPU_NUM_REGIONS];
	uint32_t RBAR_A1;
	uint32_t RLAR_A1;
	uint32_t RBAR_A2;
	uint32_t RLAR_A2;
	uint32_t RBAR_A3;
	uint32_t RLAR_A3;
	uint32_t MAIR0;
	uint32_t MAIR1;
} mpu_conf;
mpu_conf saved_mpu_conf;

static void save_mpu_state(void)
{
	uint32_t index;

	/* 
	 * MPU is divided in `MPU_NUM_REGIONS` regions.
	 * Here we save those regions configuration to be restored after PM3 entry
	 */
	for (index = 0U; index < MPU_NUM_REGIONS; index++) {
		MPU->RNR = index;
		saved_mpu_conf.RBAR[index] = MPU->RBAR;
		saved_mpu_conf.RLAR[index] = MPU->RLAR;
	}

	saved_mpu_conf.CTRL = MPU->CTRL;
	saved_mpu_conf.RNR = MPU->RNR;
	saved_mpu_conf.RBAR_A1 = MPU->RBAR_A1;
	saved_mpu_conf.RLAR_A1 = MPU->RLAR_A1;
	saved_mpu_conf.RBAR_A2 = MPU->RBAR_A2;
	saved_mpu_conf.RLAR_A2 = MPU->RLAR_A2;
	saved_mpu_conf.RBAR_A3 = MPU->RBAR_A3;
	saved_mpu_conf.RLAR_A3 = MPU->RLAR_A3;
	saved_mpu_conf.MAIR0 = MPU->MAIR0;
	saved_mpu_conf.MAIR1 = MPU->MAIR1;
}

static void restore_mpu_state(void)
{
	uint32_t index;

	for (index = 0U; index < MPU_NUM_REGIONS; index++) {
		MPU->RNR = index;
		MPU->RBAR = saved_mpu_conf.RBAR[index];
		MPU->RLAR = saved_mpu_conf.RLAR[index];
	}

	MPU->RNR = saved_mpu_conf.RNR;
	MPU->RBAR_A1 = saved_mpu_conf.RBAR_A1;
	MPU->RLAR_A1 = saved_mpu_conf.RLAR_A1;
	MPU->RBAR_A2 = saved_mpu_conf.RBAR_A2;
	MPU->RLAR_A2 = saved_mpu_conf.RLAR_A2;
	MPU->RBAR_A3 = saved_mpu_conf.RBAR_A3;
	MPU->RLAR_A3 = saved_mpu_conf.RLAR_A3;
	MPU->MAIR0 = saved_mpu_conf.MAIR0;
	MPU->MAIR1 = saved_mpu_conf.MAIR1;

	/*
	 * CTRL register must be restored last in case MPU was enabled,
	 * because some MPU registers can't be programmed while the MPU is enabled
	 */
	MPU->CTRL = saved_mpu_conf.CTRL;
}

#endif /* CONFIG_MPU */

/* Invoke Low Power/System Off specific Tasks */
__weak void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	/* Set PRIMASK */
	__disable_irq();
	/* Set BASEPRI to 0 */
	irq_unlock(0);

	POWER_DisableGDetVSensors();

	switch (state) {
	case PM_STATE_RUNTIME_IDLE:
		POWER_SetSleepMode(POWER_MODE1);
		__WFI();
		break;
	case PM_STATE_SUSPEND_TO_IDLE:
		POWER_EnterPowerMode(POWER_MODE2, &slp_cfg);
		break;
	case PM_STATE_STANDBY:
#ifdef CONFIG_MPU
		/* Save MPU state before entering PM3 */
		save_mpu_state();
#endif /* CONFIG_MPU */

		POWER_EnterPowerMode(POWER_MODE3, &slp_cfg);

#ifdef CONFIG_MPU
		/* Restore MPU as is lost after PM3 exit*/
		restore_mpu_state();
#endif /* CONFIG_MPU */

		clock_init();

		sys_clock_idle_exit();

		break;
	default:
		LOG_DBG("Unsupported power state %u", state);
		break;
	}

	if ((SOCCTRL->CHIP_INFO & SOCCIU_CHIP_INFO_REV_NUM_MASK) != 0U) {
		POWER_EnableGDetVSensors();
	}
}

/* Handle SOC specific activity after Low Power Mode Exit */
__weak void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);

	/* Clear PRIMASK */
	__enable_irq();
}

static int nxp_rw6xx_power_init(void)
{
	uint32_t suspend_sleepconfig[5] = DT_PROP_OR(NODE_ID, deep_sleep_config, {});

	slp_cfg.pm2MemPuCfg = suspend_sleepconfig[0];
	slp_cfg.pm2AnaPuCfg = suspend_sleepconfig[1];
	slp_cfg.clkGate = suspend_sleepconfig[2];
	slp_cfg.memPdCfg = suspend_sleepconfig[3];
	slp_cfg.pm3BuckCfg = suspend_sleepconfig[4];

	return 0;
}

SYS_INIT(nxp_rw6xx_power_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
