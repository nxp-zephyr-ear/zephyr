/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_gau_acomp_sensor

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/comparator.h>
#include <soc.h>

struct nxp_gau_acomp_config {
	ACOMP_Type *base;
	void (*irq_config_func)(const struct device *dev);
	uint8_t pos_input : 4;
	uint8_t neg_input : 4;
	uint8_t pos_hyst : 3;
	uint8_t neg_hyst : 3;
	uint8_t warm_time : 2;
	uint8_t bias_prog : 2;
	uint8_t edge_pulses: 2;
	uint8_t vio_scaling : 2;
	bool inactive_output : 1;
	bool rie : 1;
	bool fie : 1;
	bool invert_output : 1;
	/* Currently only IP known has 2 ACOMPs per block */
	uint8_t acomp_index : 1;
};

struct nxp_gau_acomp_data {
	sensor_trigger_handler_t callback;
	enum sensor_trigger_type trigger_type;
	uint8_t fetched_sample : 1;
	uint8_t active_mode : 1;
};

static int nxp_gau_acomp_attr_get(const struct device *dev,
					enum sensor_channel chan,
					enum sensor_attribute attr,
					struct sensor_value *val)
{
	struct nxp_gau_acomp_data *data = dev->data;

	return -ENOTSUP; /* TODO: not supported for now because triggers not tested */

	if (chan != SENSOR_CHAN_COMPARATOR && chan != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	if (attr == SENSOR_ATTR_UPPER_THRESH ||
	    attr == SENSOR_ATTR_LOWER_THRESH ||
	    attr == SENSOR_ATTR_SLOPE_TH) {
		val->val1 = data->active_mode;
		return 0;
	}

	return -EINVAL;
}

static int nxp_gau_acomp_attr_set(const struct device *dev,
					enum sensor_channel chan,
					enum sensor_attribute attr,
					const struct sensor_value *val)
{
	const struct nxp_gau_acomp_config *config = dev->config;
	ACOMP_Type *base = config->base;

	volatile uint32_t *const ctrl_reg = &base->CTRL0 +
				(config->acomp_index * sizeof(uint32_t));
	uint32_t ctrl_val = *ctrl_reg;

	return -ENOTSUP; /* TODO: not supported for now because triggers not tested */

	if (chan != SENSOR_CHAN_COMPARATOR && chan != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	switch (attr) {
	case SENSOR_ATTR_SLOPE_TH:
		if (val->val1 == SENSOR_ATTR_SLOPE_TH_RISING) {
			ctrl_val |= ACOMP_CTRL0_INT_ACT_HI_MASK;
		} else if (val->val1 == SENSOR_ATTR_SLOPE_TH_FALLING) {
			ctrl_val &= ~ACOMP_CTRL0_INT_ACT_HI_MASK;
		} else {
			return -EINVAL;
		}
		break;
	case SENSOR_ATTR_UPPER_THRESH:
	case SENSOR_ATTR_LOWER_THRESH:
		if (val->val1 == SENSOR_ATTR_THRESH_HIGH) {
			ctrl_val |= ACOMP_CTRL0_INT_ACT_HI_MASK;
		} else if (val->val1 == SENSOR_ATTR_THRESH_LOW) {
			ctrl_val &= ~ACOMP_CTRL0_INT_ACT_HI_MASK;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int nxp_gau_acomp_trigger_set(const struct device *dev,
					const struct sensor_trigger *trig,
					sensor_trigger_handler_t handler)
{
	const struct nxp_gau_acomp_config *config = dev->config;
	struct nxp_gau_acomp_data *data = dev->data;
	ACOMP_Type *base = config->base;

	volatile uint32_t *const ctrl_reg = &base->CTRL0 +
				(config->acomp_index * sizeof(uint32_t));
	uint32_t ctrl_val = *ctrl_reg;

	return -ENOTSUP; /* TODO: not supported for now because triggers not tested */

	if (trig->chan != SENSOR_CHAN_COMPARATOR && trig->chan != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	if (trig->type == SENSOR_TRIG_THRESHOLD) {
		ctrl_val &= ~ACOMP_CTRL0_EDGE_LEVL_SEL_MASK;
	} else if (trig->type == SENSOR_TRIG_DELTA) {
		ctrl_val |= ACOMP_CTRL0_EDGE_LEVL_SEL_MASK;
	} else {
		return -EINVAL;
	}

	*ctrl_reg = ctrl_val;

	data->trigger_type = trig->type;
	data->callback = handler;

	return 0;
}

static int nxp_gau_acomp_sample_fetch(const struct device *dev,
					enum sensor_channel channel)
{
	const struct nxp_gau_acomp_config *config = dev->config;
	struct nxp_gau_acomp_data *data = dev->data;
	ACOMP_Type *base = config->base;

	const volatile uint32_t *const status_reg = &base->STATUS0 +
				(config->acomp_index * sizeof(uint32_t));

	if (channel != SENSOR_CHAN_COMPARATOR && channel != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	data->fetched_sample = (*status_reg & ACOMP_STATUS0_OUT_MASK) >> ACOMP_STATUS0_OUT_SHIFT;

	return 0;
}

static int nxp_gau_acomp_channel_get(const struct device *dev,
					enum sensor_channel channel,
					struct sensor_value *val)
{
	struct nxp_gau_acomp_data *data = dev->data;

	if (channel != SENSOR_CHAN_COMPARATOR && channel != SENSOR_CHAN_ALL) {
		return -EINVAL;
	}

	val->val1 = data->fetched_sample;
	val->val2 = 0;

	return 0;
}

static int nxp_gau_acomp_init(const struct device *dev)
{
	const struct nxp_gau_acomp_config *config = dev->config;
	struct nxp_gau_acomp_data *data = dev->data;
	ACOMP_Type *base = config->base;
	volatile uint32_t *const ctrl_reg = &base->CTRL0 +
					(config->acomp_index * sizeof(uint32_t));
	uint32_t ctrl_val = 0;

	/* Configure the ACOMP */
	ctrl_val = ACOMP_CTRL0_WARMTIME(config->warm_time) |
			ACOMP_CTRL0_GPIOINV(config->invert_output) |
			ACOMP_CTRL0_HYST_SELN(config->neg_hyst) |
			ACOMP_CTRL0_HYST_SELP(config->pos_hyst) |
			ACOMP_CTRL0_BIAS_PROG(config->bias_prog) |
			ACOMP_CTRL0_INACT_VAL(config->inactive_output) |
			ACOMP_CTRL0_LEVEL_SEL(config->vio_scaling) |
			ACOMP_CTRL0_RIE(config->rie) |
			ACOMP_CTRL0_FIE(config->fie) |
			ACOMP_CTRL0_POS_SEL(config->pos_input) |
			ACOMP_CTRL0_NEG_SEL(config->neg_input) |
			ACOMP_CTRL0_INT_ACT_HI_MASK |
			ACOMP_CTRL0_MUXEN_MASK;
	*ctrl_reg = ctrl_val;

	/* keeping reset value as default for attributes */
	data->active_mode = SENSOR_ATTR_THRESH_HIGH;

	/* Enable the ACOMP (must happen after configuring MUXEN) */
	*ctrl_reg |= ACOMP_CTRL0_EN_MASK;

	return 0;
}

static void nxp_gau_acomp_isr(const struct device *dev)
{
	struct nxp_gau_acomp_data *data = dev->data;
	struct sensor_trigger trigger;

	return; /* TODO: not supported for now because triggers not tested */

	trigger.type = data->trigger_type;
	trigger.chan = SENSOR_CHAN_COMPARATOR;

	data->callback(dev, &trigger);
}

static const struct sensor_driver_api nxp_gau_acomp_driver_api = {
	.attr_get = nxp_gau_acomp_attr_get,
	.attr_set = nxp_gau_acomp_attr_set,
	.trigger_set = nxp_gau_acomp_trigger_set,
	.sample_fetch = nxp_gau_acomp_sample_fetch,
	.channel_get = nxp_gau_acomp_channel_get,
};

#define NXP_GAU_ACOMP_CONFIG(n)							\
	{									\
		.base = (ACOMP_Type *)DT_REG_ADDR(DT_INST_PARENT(n)),		\
		.warm_time = DT_INST_ENUM_IDX_OR(n, nxp_warmtime, 0),		\
		.bias_prog = DT_INST_ENUM_IDX_OR(n, nxp_bias_prog, 0),		\
		.inactive_output = DT_INST_PROP(n, nxp_inactive_output),	\
		.rie = DT_INST_ENUM_IDX_OR(n, nxp_edge_pulses, 0)		\
				& 0b01 ? true : false,				\
		.fie = DT_INST_ENUM_IDX_OR(n, nxp_edge_pulses, 0)		\
				& 0b10 ? true : false,				\
		.edge_pulses = DT_INST_ENUM_IDX_OR(n, nxp_edge_pulses, 0),	\
		.vio_scaling = DT_INST_ENUM_IDX_OR(n,				\
						nxp_vio_scaling_factor, 0),	\
		.invert_output = DT_INST_PROP(n, nxp_invert_output),		\
		.pos_input = DT_INST_ENUM_IDX(n, nxp_pos_input),		\
		.neg_input = DT_INST_ENUM_IDX(n, nxp_neg_input),		\
		.pos_hyst = DT_INST_ENUM_IDX_OR(n, nxp_pos_hysteresis, 0),	\
		.neg_hyst = DT_INST_ENUM_IDX_OR(n, nxp_neg_hysteresis, 0),	\
		.acomp_index = DT_INST_REG_ADDR(n),				\
		.irq_config_func = nxp_gau_acomp_irq_config_func_##n,		\
	}

#define NXP_GAU_ACOMP_IRQ_CONFIG_FUNC_DEFINITION(n)				\
	static void nxp_gau_acomp_irq_config_func_##n(const struct device *dev)	\
	{									\
		IRQ_CONNECT(DT_IRQN(DT_INST_PARENT(n)),				\
				DT_IRQ(DT_INST_PARENT(n), priority),		\
				nxp_gau_acomp_isr, DEVICE_DT_INST_GET(n), 0);	\
		irq_enable(DT_IRQN(DT_INST_PARENT(n)));				\
	}


#define NXP_GAU_ACOMP_INIT(n)							\
										\
	NXP_GAU_ACOMP_IRQ_CONFIG_FUNC_DEFINITION(n)				\
										\
	static struct nxp_gau_acomp_data nxp_gau_acomp_data_##n;		\
										\
	static const struct nxp_gau_acomp_config nxp_gau_acomp_config_##n =	\
					NXP_GAU_ACOMP_CONFIG(n);		\
										\
	SENSOR_DEVICE_DT_INST_DEFINE(n, &nxp_gau_acomp_init, NULL,		\
					&nxp_gau_acomp_data_##n,		\
					&nxp_gau_acomp_config_##n,		\
					POST_KERNEL,				\
					CONFIG_SENSOR_INIT_PRIORITY,		\
					&nxp_gau_acomp_driver_api);		\


DT_INST_FOREACH_STATUS_OKAY(NXP_GAU_ACOMP_INIT)
