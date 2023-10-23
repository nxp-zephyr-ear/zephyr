/*
 * Copyright 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_gau_adc

#include <errno.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nxp,gau-adc.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
LOG_MODULE_REGISTER(adc_mcux_gau_adc);

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#include "fsl_adc.h"

#ifndef CONFIG_ADC_CONFIGURABLE_INPUTS
#error "ADC requires configuurable inputs"
#endif

struct mcux_gau_adc_config {
	ADC_Type *base;
	void (*irq_config_func)(const struct device *dev);
	adc_clock_divider_t clock_div;
	adc_analog_portion_power_mode_t power_mode;
	adc_warm_up_time_t warmup_time;
	adc_trigger_source_t trigger;
	bool input_gain_buffer;
	adc_result_width_t result_width;
	adc_fifo_threshold_t fifo_threshold;
	bool enable_dma;
	adc_calibration_ref_t cal_ref;
	adc_resolution_t resolution;
	adc_average_length_t oversampling;
};

struct mcux_gau_adc_data {
	const struct device *dev;
	struct adc_context ctx;
	adc_channel_source_t channel_sources[16];
	uint8_t scan_length;
	uint16_t *results;
};

static int mcux_gau_adc_channel_setup(const struct device *dev,
				      const struct adc_channel_cfg *channel_cfg)
{
	const struct mcux_gau_adc_config *config = dev->config;
	struct mcux_gau_adc_data *data = dev->data;
	adc_input_gain_t gain;
	adc_vref_source_t ref;
	adc_warm_up_time_t warmup;
	ADC_Type *base = config->base;
	uint8_t channel_id = channel_cfg->channel_id;
	uint8_t source_channel = channel_cfg->input_positive;

	/* Input validations */

	if (channel_cfg->differential) {
		LOG_ERR("Differential channels not yet supported");
		return -ENOTSUP;
	}

	if (channel_id >= 16) {
		LOG_ERR("ADC does not support more than 16 channels");
		return -ENOTSUP;
	}

	if (source_channel < 0 || (source_channel > 12 && source_channel != 15)) {
		LOG_ERR("Invalid source channel");
		return -EINVAL;
	}

	/* Set Acquisition/Warmup time */
	LOG_DBG("Acquisition/Warmup time is global to entire ADC peripheral, "
		 "i.e. channel_setup will override this property for all previous channels.");
	base->ADC_REG_INTERVAL &= ~ADC_ADC_REG_INTERVAL_WARMUP_TIME_MASK;
	if (channel_cfg->acquisition_time >= 32) {
		warmup = kADC_WarmUpStateBypass;
		LOG_DBG("ADC warmup is bypassed");
	} else {
		warmup = channel_cfg->acquisition_time;
	}
	base->ADC_REG_INTERVAL |= ADC_ADC_REG_INTERVAL_WARMUP_TIME(warmup);

	/* Set Input Gain */
	LOG_DBG("Input gain is global to entire ADC peripheral, "
		"i.e. channel_setup will override this property for all previous channels.");
	base->ADC_REG_ANA &= ~ADC_ADC_REG_ANA_INBUF_GAIN_MASK;
	if (channel_cfg->gain == ADC_GAIN_1) {
		gain = kADC_InputGain1;
	} else if (channel_cfg->gain == ADC_GAIN_1_2) {
		gain = kADC_InputGain0P5;
	} else if (channel_cfg->gain == ADC_GAIN_2) {
		gain = kADC_InputGain2;
	} else {
		LOG_ERR("Invalid gain");
		return -EINVAL;
	}
	base->ADC_REG_ANA |= ADC_ADC_REG_ANA_INBUF_GAIN(gain);

	/* Set Reference voltage of ADC */
	LOG_DBG("Reference voltage is global to entire ADC peripheral, "
		"i.e. channel_setup will override this property for all previous channels.");
	base->ADC_REG_ANA &= ~ADC_ADC_REG_ANA_VREF_SEL_MASK;
	if (channel_cfg->reference == ADC_REF_INTERNAL) {
		ref = kADC_Vref1P2V;
	} else if (channel_cfg->reference == ADC_REF_EXTERNAL0) {
		ref = kADC_VrefExternal;
	} else if (channel_cfg->reference == ADC_REF_VDD_1) {
		ref = kADC_Vref1P8V;
	} else {
		LOG_ERR("Vref not supported");
		return -ENOTSUP;
	}
	base->ADC_REG_ANA |= ADC_ADC_REG_ANA_VREF_SEL(ref);

	data->channel_sources[channel_id] = source_channel;

	return 0;
}

static void mcux_gau_adc_isr(const struct device *dev)
{
	const struct mcux_gau_adc_config *config = dev->config;
	struct mcux_gau_adc_data *data = dev->data;
	ADC_Type *base = config->base;

	if ((ADC_GetStatusFlags(base) & kADC_DataReadyInterruptFlag) != 0UL) {
		uint8_t fifo_count = (base->ADC_REG_STATUS &
					ADC_ADC_REG_STATUS_FIFO_DATA_COUNT_MASK) >>
					ADC_ADC_REG_STATUS_FIFO_DATA_COUNT_SHIFT;

		if (fifo_count > data->scan_length) {
			LOG_ERR("Unexpected ADC FIFO behaviour");
			return;
		}

		while (base->ADC_REG_STATUS & ADC_ADC_REG_STATUS_FIFO_NE_MASK) {
			*(data->results++) = ADC_GetConversionResult(base);
		}

		ADC_StopConversion(base);
		ADC_ClearStatusFlags(base, kADC_DataReadyInterruptFlag);
		adc_context_on_sampling_done(&data->ctx, dev);
	} else {
		LOG_WRN("ADC received unexpected interrupt, "
			"REG_ISR: %x", base->ADC_REG_ISR);
	}

}

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct mcux_gau_adc_data *data =
		CONTAINER_OF(ctx, struct mcux_gau_adc_data, ctx);
	const struct mcux_gau_adc_config *config = data->dev->config;
	ADC_Type *base = config->base;

	ADC_DoSoftwareTrigger(base);
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx,
					      bool repeat_sampling)
{
	if (repeat_sampling) {
		LOG_DBG("ADC driver does not overwrite samplings within a sequence");
	}
}

static int do_read(const struct device *dev,
		   const struct adc_sequence *sequence)
{
	const struct mcux_gau_adc_config *config = dev->config;
	ADC_Type *base = config->base;
	struct mcux_gau_adc_data *data = dev->data;
	adc_resolution_t res;
	adc_average_length_t avg;
	uint8_t num_channels = 0;

	/* Only 16 channels on the ADC */
	if (sequence->channels & (0xFFFF << 16)) {
		LOG_ERR("Invalid channels selected for sequence");
		return -EINVAL;
	}

	/* Count channels */
	for (int i = 0; i < 16; i++) {
		num_channels += ((sequence->channels & (0x1 << i)) ? 1 : 0);
	}

	/* Buffer must hold (number of samples per channel) * (number of channels) samples */
	if (sequence->options != NULL &&
		sequence->buffer_size < ((1 + sequence->options->extra_samplings) * num_channels)) {
		LOG_ERR("Buffer size too small");
		return -ENOMEM;
	} else if (sequence->options == NULL && sequence->buffer_size < num_channels) {
		LOG_ERR("Buffer size too small");
		return -ENOMEM;
	}

	/* Set scan length in data struct for isr to understand & set scan length register */
	base->ADC_REG_CONFIG &= ~ADC_ADC_REG_CONFIG_SCAN_LENGTH_MASK;
	data->scan_length = num_channels;
	/* Register Value is 1 less than what it represents */
	base->ADC_REG_CONFIG |= ADC_ADC_REG_CONFIG_SCAN_LENGTH(data->scan_length - 1);


	/* Set up scan channels */
	for (int channel = 0; channel < 16; channel++) {
		if (sequence->channels & (0x1 << channel)) {
			ADC_SetScanChannel(base,
				data->scan_length - num_channels--,
				data->channel_sources[channel]);
		}
	}

	/* Set resolution of ADC */
	base->ADC_REG_ANA &= ~ADC_ADC_REG_ANA_RES_SEL_MASK;
	switch (sequence->resolution) {
	case 12:
		res = kADC_Resolution12Bit;
		break;
	case 14:
		res = kADC_Resolution14Bit;
		break;
	case 16:
		res = kADC_Resolution16Bit;
		break;
	default:
		LOG_ERR("Invalid resolution");
		return -EINVAL;
	}
	base->ADC_REG_ANA |= ADC_ADC_REG_ANA_RES_SEL(res);

	/* Set oversampling */
	base->ADC_REG_CONFIG &= ~ADC_ADC_REG_CONFIG_AVG_SEL_MASK;
	switch (sequence->oversampling) {
	case 0:
		avg = kADC_AverageNone;
		break;
	case 1:
		avg = kADC_Average2;
		break;
	case 2:
		avg = kADC_Average4;
		break;
	case 3:
		avg = kADC_Average8;
		break;
	case 4:
		avg = kADC_Average16;
		break;
	default:
		LOG_ERR("Invalid oversampling setting");
		return -EINVAL;
	}
	base->ADC_REG_CONFIG |= ADC_ADC_REG_CONFIG_AVG_SEL(avg);

	/* Calibrate if requested */
	if (sequence->calibrate) {
		if (ADC_DoAutoCalibration(base, config->cal_ref)) {
			LOG_WRN("Calibration of ADC failed!");
		}
	}

	data->results = sequence->buffer;

	if (config->trigger == kADC_TriggerSourceSoftware) {
		ADC_DoSoftwareTrigger(base);
	}

	while (!(base->ADC_REG_CMD & ADC_ADC_REG_CMD_CONV_START_MASK))
		;

	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int mcux_gau_adc_read(const struct device *dev,
			   const struct adc_sequence *sequence)
{
	struct mcux_gau_adc_data *data = dev->data;
	int error;

	adc_context_lock(&data->ctx, false, NULL);
	error = do_read(dev, sequence);
	adc_context_release(&data->ctx, error);
	return error;
}

#ifdef CONFIG_ADC_ASYNC
static int mcux_gau_adc_read_async(const struct device *dev,
				   const struct adc_sequence *sequence,
				   struct k_poll_signal *async)
{
	struct mcux_gau_adc_data *data = dev->data;
	int error;

	adc_context_lock(&data->ctx, true, async);
	error = do_read(dev, sequence);
	adc_context_release(&data->ctx, error);
	return error;
}
#endif


static int mcux_gau_adc_init(const struct device *dev)
{
	const struct mcux_gau_adc_config *config = dev->config;
	struct mcux_gau_adc_data *data = dev->data;
	ADC_Type *base = config->base;
	adc_config_t adc_config;

	data->dev = dev;

	LOG_DBG("Initializing ADC");

	if (config->trigger == kADC_TriggerSourceGpt) {
		LOG_ERR("GPT Trigger currently not supported");
		return -ENOTSUP;
	}

	if (config->enable_dma) {
		LOG_ERR("ADC DMA not yet supported");
		return -ENOTSUP;
	}

	ADC_GetDefaultConfig(&adc_config);

	adc_config.clockDivider = config->clock_div;
	adc_config.powerMode = config->power_mode;
	adc_config.warmupTime = config->warmup_time;
	adc_config.inputMode = kADC_InputSingleEnded;
	adc_config.conversionMode = kADC_ConversionOneShot;
	adc_config.averageLength = config->oversampling;
	adc_config.triggerSource = config->trigger;
	adc_config.enableInputGainBuffer = config->input_gain_buffer;
	adc_config.resultWidth = config->result_width;
	adc_config.fifoThreshold = config->fifo_threshold;
	adc_config.enableDMA = config->enable_dma;
	adc_config.resolution = config->resolution;
	adc_config.enableADC = true;

	ADC_Init(base, &adc_config);

	if (ADC_DoAutoCalibration(base, config->cal_ref)) {
		LOG_WRN("Calibration of ADC failed!");
	}

	ADC_ClearStatusFlags(base, kADC_DataReadyInterruptFlag);

	config->irq_config_func(dev);
	ADC_EnableInterrupts(base, kADC_DataReadyInterruptEnable);

	adc_context_init(&data->ctx);
	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}


static const struct adc_driver_api mcux_gau_adc_driver_api = {
	.channel_setup = mcux_gau_adc_channel_setup,
	.read = mcux_gau_adc_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = mcux_gau_adc_read_async,
#endif
};

#define GAU_ADC_MCUX_INIT(n)							\
										\
	static void mcux_gau_adc_config_func_##n(const struct device *dev);     \
										\
	static const struct mcux_gau_adc_config mcux_gau_adc_config_##n = {	\
		.base = (ADC_Type *)DT_INST_REG_ADDR(n),			\
		.irq_config_func = mcux_gau_adc_config_func_##n,		\
		/* Minus one because DT starts at 1, HAL enum starts at 0 */	\
		.clock_div = DT_INST_PROP(n, clock_divider) - 1,		\
		.power_mode = DT_INST_ENUM_IDX(n, power_mode),			\
		/* Minus one because DT starts at 1, HAL enum starts at 0 */	\
		.warmup_time = DT_INST_PROP(n, warmup_time) - 1,		\
		.trigger = DT_INST_ENUM_IDX(n, trigger_source),			\
		.result_width = DT_INST_ENUM_IDX(n, result_width),		\
		.fifo_threshold = DT_INST_ENUM_IDX(n, fifo_threshold),		\
		.input_gain_buffer = DT_INST_PROP(n, input_buffer),		\
		.enable_dma = DT_INST_PROP(n, enable_dma),			\
		.cal_ref = (DT_INST_PROP(n, ext_cal_volt) ? 1 : 0),		\
		.resolution = DT_INST_ENUM_IDX(n, resolution),			\
		.oversampling = DT_INST_ENUM_IDX(n, oversampling),		\
	};									\
										\
	static struct mcux_gau_adc_data mcux_gau_adc_data_##n = {0};		\
										\
	DEVICE_DT_INST_DEFINE(n, &mcux_gau_adc_init, NULL,			\
			      &mcux_gau_adc_data_##n, &mcux_gau_adc_config_##n,	\
			      POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,		\
			      &mcux_gau_adc_driver_api);			\
										\
	static void mcux_gau_adc_config_func_##n(const struct device *dev)	\
	{									\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),		\
				mcux_gau_adc_isr, DEVICE_DT_INST_GET(n), 0);	\
		irq_enable(DT_INST_IRQN(n));					\
	}

DT_INST_FOREACH_STATUS_OKAY(GAU_ADC_MCUX_INIT)
