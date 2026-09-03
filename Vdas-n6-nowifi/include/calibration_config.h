#ifndef CALIBRATION_CONFIG_H
#define CALIBRATION_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define DAQ_NUM_ADC_CHANNELS 6
#define DAQ_NUM_DAC_CHANNELS 4
#define DAQ_NUM_PID_LOOPS    2

static const uint8_t ADC_LOGICAL_TO_PHYSICAL_MAP[DAQ_NUM_ADC_CHANNELS] = {
    5, 0, 1, 2, 3, 4
};

#define PID0_DEFAULT_INPUT_CHANNEL   0  /* ADC CH0 */
#define PID0_DEFAULT_OUTPUT_CHANNEL  0  /* DAC A   */

#define PID1_DEFAULT_INPUT_CHANNEL   1  /* ADC CH1 */
#define PID1_DEFAULT_OUTPUT_CHANNEL  1  /* DAC B   */

/* Equation: Voltage = m * RawCode + c */
static inline float CALIB_AdcCountToVoltage(uint8_t logical_channel, uint16_t raw_code)
{
    static const float adc_slope[DAQ_NUM_ADC_CHANNELS] = {
        0.00183189655f, 0.00183189655f, 0.00183189655f,
        0.00183189655f, 0.00183189655f, 0.00183189655f
    };
    static const float adc_offset[DAQ_NUM_ADC_CHANNELS] = {
        -0.376616379f, -0.376616379f, -0.376616379f,
        -0.376616379f, -0.376616379f, -0.376616379f
    };

    if (logical_channel >= DAQ_NUM_ADC_CHANNELS) logical_channel = 0;
    
    float voltage = (adc_slope[logical_channel] * (float)raw_code) + adc_offset[logical_channel];
    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > 5.0f) voltage = 5.0f;
    return voltage;
}

static inline float CALIB_AdcVoltageToCurrent(uint8_t logical_channel, float voltage)
{
    static const float i2v_slope[DAQ_NUM_ADC_CHANNELS] = { 3.200f, 3.200f, 3.200f, 3.200f, 3.200f, 3.200f };
    static const float i2v_offset[DAQ_NUM_ADC_CHANNELS] = { 4.000f, 4.000f, 4.000f, 4.000f, 4.000f, 4.000f };

    if (logical_channel >= DAQ_NUM_ADC_CHANNELS) logical_channel = 0;
    return (i2v_slope[logical_channel] * voltage) + i2v_offset[logical_channel];
}

static inline float CALIB_DacCurrentToVoltage(uint8_t dac_channel, float current_mA)
{
    static const float v2i_slope[DAQ_NUM_DAC_CHANNELS] = { 0.3125f, 0.3125f, 0.3125f, 0.3125f };
    static const float v2i_offset[DAQ_NUM_DAC_CHANNELS] = { -1.250f, -1.250f, -1.250f, -1.250f };

    if (dac_channel >= DAQ_NUM_DAC_CHANNELS) dac_channel = 0;
    return (v2i_slope[dac_channel] * current_mA) + v2i_offset[dac_channel];
}

static inline float CALIB_DacVoltageToCurrent(uint8_t dac_channel, float voltage)
{
    static const float dac_readback_slope[DAQ_NUM_DAC_CHANNELS] = { 3.200f, 3.200f, 3.200f, 3.200f };
    static const float dac_readback_offset[DAQ_NUM_DAC_CHANNELS] = { 4.000f, 4.000f, 4.000f, 4.000f };

    if (dac_channel >= DAQ_NUM_DAC_CHANNELS) dac_channel = 0;
    return (dac_readback_slope[dac_channel] * voltage) + dac_readback_offset[dac_channel];
}

static inline uint16_t CALIB_DacVoltageToCode(uint8_t dac_channel, float voltage)
{
    #define DAC_MAX_COUNT 4095.0f
    #define DAC_REF_VOLT  5.0f

    static const float dac_code_slope[DAQ_NUM_DAC_CHANNELS] = {
        (DAC_MAX_COUNT / DAC_REF_VOLT), (DAC_MAX_COUNT / DAC_REF_VOLT),
        (DAC_MAX_COUNT / DAC_REF_VOLT), (DAC_MAX_COUNT / DAC_REF_VOLT)
    };

    if (dac_channel >= DAQ_NUM_DAC_CHANNELS) dac_channel = 0;
    float code_f = (dac_code_slope[dac_channel] * voltage) + 0.5f;
    if (code_f < 0.0f) code_f = 0.0f;
    if (code_f > DAC_MAX_COUNT) code_f = DAC_MAX_COUNT;
    return (uint16_t)code_f;
}

#endif // CALIBRATION_CONFIG_H