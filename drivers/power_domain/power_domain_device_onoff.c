/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(power_domain_deviceonoff, CONFIG_POWER_DOMAIN_LOG_LEVEL);

/* Indicates the end of the onoff_power_states array for count purposes */
#define POWER_DOMAIN_DEVICE_ONOFF_STATE_COUNT	0xFF

struct pd_deviceonoff_config {
	uint8_t *onoff_power_states;
};

struct pd_visitor_context {
	const struct device *domain;
	enum pm_device_action action;
};

static int pd_domain_visitor(const struct device *dev, void *context)
{
	struct pd_visitor_context *visitor_context = context;

	/* Only run action if the device is on the specified domain */
	if (!dev->pm || (dev->pm->domain != visitor_context->domain)) {
		return 0;
	}

	/* In case device is active, first suspend it before turning it off */
	if ((visitor_context->action == PM_DEVICE_ACTION_TURN_OFF) &&
		(dev->pm->state == PM_DEVICE_STATE_ACTIVE)) {
		(void)pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	}
	(void)pm_device_action_run(dev, visitor_context->action);
	return 0;
}

static int pd_pm_action(const struct device *dev, enum pm_device_action action)
{
	const struct pd_deviceonoff_config *config = dev->config;
	uint8_t i = 0;
	enum pm_state state = pm_state_next_get(0)->state;
	struct pd_visitor_context context = {.domain = dev};

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		while (config->onoff_power_states[i] != POWER_DOMAIN_DEVICE_ONOFF_STATE_COUNT) {
			if (state == config->onoff_power_states[i]) {
				/* Notify devices on the domain they are now powered */
				context.action = PM_DEVICE_ACTION_TURN_ON;
				(void)device_supported_foreach(dev, pd_domain_visitor, &context);
				/* No need to go through the rest of the array of states */
				break;
			}
			i++;
		}
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		while (config->onoff_power_states[i] != POWER_DOMAIN_DEVICE_ONOFF_STATE_COUNT) {
			if (state == config->onoff_power_states[i]) {
				/* Notify devices on the domain that power is going down */
				context.action = PM_DEVICE_ACTION_TURN_OFF;
				(void)device_supported_foreach(dev, pd_domain_visitor, &context);
				/* No need to go through the rest of the array of states */
				break;
			}
			i++;
		}
		break;
	case PM_DEVICE_ACTION_TURN_ON:
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

#define DT_DRV_COMPAT power_domain_deviceonoff

#define PM_STATE_FROM_DT(i, node_id, prop_name)						\
	COND_CODE_1(DT_NODE_HAS_STATUS(DT_PHANDLE_BY_IDX(node_id, prop_name, i), okay),	\
		(PM_STATE_DT_INIT(DT_PHANDLE_BY_IDX(node_id, prop_name, i)),), ())

#define PM_STATE_FROM_DT_2(i, node_id, prop_name)					\
		PM_STATE_DT_INIT(DT_PHANDLE_BY_IDX(node_id, prop_name, i))

#define POWER_DOMAIN_DEVICE_ONOFF_STATES(inst, node_id)					\
	uint8_t onoff_states_##inst[] = {						\
		LISTIFY(DT_PROP_LEN_OR(node_id, onoff_power_states, 0),			\
			PM_STATE_FROM_DT, (), node_id, onoff_power_states)		\
		POWER_DOMAIN_DEVICE_ONOFF_STATE_COUNT					\
	};

#define POWER_DOMAIN_DEVICE(id)								\
	POWER_DOMAIN_DEVICE_ONOFF_STATES(id, DT_DRV_INST(id))				\
											\
	static const struct pd_deviceonoff_config pd_deviceonoff_##id##_cfg = {		\
		.onoff_power_states = onoff_states_##id,				\
	};										\
	PM_DEVICE_DT_INST_DEFINE(id, pd_pm_action);					\
	DEVICE_DT_INST_DEFINE(id, NULL, PM_DEVICE_DT_INST_GET(id),			\
			      NULL, &pd_deviceonoff_##id##_cfg, PRE_KERNEL_1,		\
			      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(POWER_DOMAIN_DEVICE)
