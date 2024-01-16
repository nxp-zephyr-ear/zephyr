/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NXP
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NXP_GAU_ADC_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NXP_GAU_ADC_H_

#include <zephyr/dt-bindings/adc/adc.h>

/*
 * These values are not arbitrary!
 * A lot of these values map directly to enum values in
 * the NXP Hal driver "cns_adc" header file, be careful
 * about changing them, the values are used directly by the
 * Zephyr NXP GAU ADC Driver.
 */

/* Power Modes */
#define GAU_ADC_FULL_BIAS_CURRENT 0
#define GAU_ADC_HALF_BIAS_CURRENT 1

/* Warmup bypass */
#define GAU_ADC_WARMUP_BYPASS 0x20 + 1 /* Driver expects value offset one */

/* Input Modes */
#define GAU_ADC_SINGLE 0
#define GAU_ADC_DIFFERENTIAL 1

/* Conversion Modes */
#define GAU_ADC_ONE_SHOT 0
#define GAU_ADC_CONTINUOUS 1

/* Average Lengths */
#define GAU_ADC_AVG_NONE 0
#define GAU_ADC_AVG_TWO 1
#define GAU_ADC_AVG_FOUR 2
#define GAU_ADC_AVG_EIGHT 3
#define GAU_ADC_AVG_SIXTEEN 4

/* Trigger Sources */
#define GAU_ADC_TRIGGER_GPT 0
#define GAU_ADC_TRIGGER_ACOMP 1
#define GAU_ADC_TRIGGER_GPIO40 2
#define GAU_ADC_TRIGGER_GPIO41 3
#define GAU_ADC_TRIGGER_SOFTWARE 4

/* Result Width */
#define GAU_ADC_RES_WIDTH_16 0
#define GAU_ADC_RES_WIDTH_32 1

/* Fifo Threshold */
#define GAU_ADC_FIFO_THRESHOLD_ONE 0
#define GAU_ADC_FIFO_THRESHOLD_FOUR 1
#define GAU_ADC_FIFO_THRESHOLD_EIGHT 2
#define GAU_ADC_FIFO_THRESHOLD_SIXTEEN 3

/* Resolution */
#define GAU_ADC_RESOLUTION_12BIT 0
#define GAU_ADC_RESOLUTION_14BIT 1
#define GAU_ADC_RESOLUTION_16BIT 2

/* Channel Sources (reg property) */
#define GAU_ADC_CH0 0
#define GAU_ADC_CH1 1
#define GAU_ADC_CH2 2
#define GAU_ADC_CH3 3
#define GAU_ADC_CH4 4
#define GAU_ADC_CH5 5
#define GAU_ADC_CH6 6
#define GAU_ADC_CH7 7
#define GAU_ADC_VBATS 8
#define GAU_ADC_VREF 9
#define GAU_ADC_DACA 10
#define GAU_ADC_DACB 11
#define GAU_ADC_VSSA 12
#define GAU_ADC_TEMPP 15

/* Input buffer gain */
#define GAU_ADC_INPUTGAIN0P5 0
#define GAU_ADC_INPUTGAIN1 1
#define GAU_ADC_INPUTGAIN2 2

/* Reference voltage */
#define GAU_ADC_VREF_1_8_V 0
#define GAU_ADC_VREF_1_2_V 1
#define GAU_ADC_VREF_EXTERNAL 2
#define GAU_ADC_VREF_1_2_V_CAP 3

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NXP_GAU_ADC_H_ */
