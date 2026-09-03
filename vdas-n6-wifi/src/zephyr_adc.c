#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <stdint.h>
#include <stdbool.h>
#include "drivers_adc.h"
#include "calibration_config.h"

#define ADC_PHYSICAL_CHANNELS 6

static const struct adc_dt_spec adc_ch[ADC_PHYSICAL_CHANNELS] = {
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 4),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 5),
};

void ADC_Init(void)
{
    for (int i = 0; i < ADC_PHYSICAL_CHANNELS; i++) {
        if (!adc_is_ready_dt(&adc_ch[i])) {
            printk("ADC physical channel %d not ready\n", i);
            return;
        }
        int ret = adc_channel_setup_dt(&adc_ch[i]);
        if (ret < 0) {
            printk("ADC physical channel %d setup failed: %d\n", i, ret);
            return;
        }
    }
    printk("ADC initialized with dynamic calibration mapping\n");
}

uint16_t ADC_ReadRaw(uint8_t logical_channel)
{
    if (logical_channel >= DAQ_NUM_ADC_CHANNELS) {
        return 0;
    }

    uint8_t physical_channel = ADC_LOGICAL_TO_PHYSICAL_MAP[logical_channel];
    if (physical_channel >= ADC_PHYSICAL_CHANNELS) {
        return 0;
    }

    uint32_t buf = 0;
    struct adc_sequence seq = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    if (adc_sequence_init_dt(&adc_ch[physical_channel], &seq) < 0) {
        return 0;
    }

    if (adc_read_dt(&adc_ch[physical_channel], &seq) < 0) {
        return 0;
    }

    return (uint16_t)(buf & 0xFFFF);
}

float ADC_ReadVoltage(uint8_t logical_channel)
{
    if (logical_channel >= DAQ_NUM_ADC_CHANNELS) {
        return 0.0f;
    }

    uint16_t raw = ADC_ReadRaw(logical_channel);
    return CALIB_AdcCountToVoltage(logical_channel, raw);
}

float ADC_ReadCurrent(uint8_t logical_channel)
{
    if (logical_channel >= DAQ_NUM_ADC_CHANNELS) {
        return 0.0f;
    }

    float voltage = ADC_ReadVoltage(logical_channel);
    return CALIB_AdcVoltageToCurrent(logical_channel, voltage);
}

void ADC_SetMockValue(uint8_t channel, float voltage)
{
    (void)channel;
    (void)voltage;
}

const struct device *ADC_GetDevice(void)
{
    return adc_ch[0].dev;
}