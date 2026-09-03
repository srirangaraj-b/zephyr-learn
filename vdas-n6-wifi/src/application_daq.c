#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "application_daq.h"
#include "drivers_adc.h"
#include "drivers_dac.h"
#include "control_pid.h"
#include "calibration_config.h"

void DAQ_Init(daq_state_t *daq)
{
    if (!daq) return;
    
    strcpy(daq->manufacturer, "VIMICRO");
    strcpy(daq->model, "VDAS");
    strcpy(daq->serial, "01E");
    strcpy(daq->version, "0.1");
    
    /* CH0 & CH1 default to CURRENT; CH2..CH5 default to VOLTAGE */
    for (int i = 0; i < DAQ_NUM_ADC_CHANNELS; i++) {
        daq->channels[i].mode = (i < 2) ? CHANNEL_MODE_CURRENT : CHANNEL_MODE_VOLTAGE;
        daq->channels[i].last_measurement = 0.0f;
    }
    
    daq->sample_rate = 100;
    daq->sample_count = 1;
    
    for (int ch = 0; ch < DAQ_NUM_DAC_CHANNELS; ch++) {
        daq->output_enabled[ch] = false;
        daq->dac_output[ch] = 0.0f;
    }
    daq->active_dac_channel = DAC_CHANNEL_A;
    
    /* Loop 0 Defaults */
    daq->pid_input_channel[0] = PID0_DEFAULT_INPUT_CHANNEL;
    daq->pid_output_channel[0] = PID0_DEFAULT_OUTPUT_CHANNEL;
    PID_Init(&daq->pid[0]);
    PID_SetOutputLimits(&daq->pid[0], 4.0f, 20.0f);
    
    /* Loop 1 Defaults */
    daq->pid_input_channel[1] = PID1_DEFAULT_INPUT_CHANNEL;
    daq->pid_output_channel[1] = PID1_DEFAULT_OUTPUT_CHANNEL;
    PID_Init(&daq->pid[1]);
    PID_SetOutputLimits(&daq->pid[1], 4.0f, 20.0f);
}

int DAQ_ConfigChannel_Voltage(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_ADC_CHANNELS) return -1;
    daq->channels[channel].mode = CHANNEL_MODE_VOLTAGE;
    return 0;
}

int DAQ_ConfigChannel_Current(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_ADC_CHANNELS) return -1;
    daq->channels[channel].mode = CHANNEL_MODE_CURRENT;
    return 0;
}

channel_mode_t DAQ_GetChannelMode(const daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_ADC_CHANNELS) return CHANNEL_MODE_VOLTAGE;
    return daq->channels[channel].mode;
}

float DAQ_ReadChannel(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_ADC_CHANNELS) return 0.0f;
    
    float measurement = 0.0f;
    if (daq->channels[channel].mode == CHANNEL_MODE_VOLTAGE) {
        measurement = ADC_ReadVoltage(channel);
    } else {
        measurement = ADC_ReadCurrent(channel);
    }
    daq->channels[channel].last_measurement = measurement;
    return measurement;
}

void DAQ_SetSampleRate(daq_state_t *daq, uint32_t rate) { if (daq) daq->sample_rate = rate; }
uint32_t DAQ_GetSampleRate(const daq_state_t *daq) { return daq ? daq->sample_rate : 100; }
void DAQ_SetSampleCount(daq_state_t *daq, uint32_t count) { if (daq) daq->sample_count = count; }
uint32_t DAQ_GetSampleCount(const daq_state_t *daq) { return daq ? daq->sample_count : 1; }

bool DAQ_IsAnyPIDDrivingDAC(const daq_state_t *daq, uint8_t dac_channel)
{
    if (!daq) return false;
    for (int p = 0; p < DAQ_NUM_PID_LOOPS; p++) {
        if (PID_IsEnabled(&daq->pid[p]) && daq->pid_output_channel[p] == dac_channel) {
            return true;
        }
    }
    return false;
}

void DAQ_EnableOutput(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return;
    
    for (int p = 0; p < DAQ_NUM_PID_LOOPS; p++) {
        if (daq->pid_output_channel[p] == channel && PID_IsEnabled(&daq->pid[p])) {
            PID_Disable(&daq->pid[p]);
        }
    }
    
    daq->output_enabled[channel] = true;
}

void DAQ_DisableOutput(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return;
    daq->output_enabled[channel] = false;
    DAC_SetVoltage(channel, 0.0f);
}

bool DAQ_IsOutputEnabled(const daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return false;
    return daq->output_enabled[channel];
}

int DAQ_EnablePID(daq_state_t *daq, uint8_t pid_index)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS) return -1;
    
    uint8_t in_ch = daq->pid_input_channel[pid_index];
    uint8_t out_ch = daq->pid_output_channel[pid_index];
    daq->output_enabled[out_ch] = false;
    
    /* Dynamically configure limits based on input mode */
    if (daq->channels[in_ch].mode == CHANNEL_MODE_CURRENT) {
        PID_SetOutputLimits(&daq->pid[pid_index], 4.0f, 20.0f);
    } else {
        PID_SetOutputLimits(&daq->pid[pid_index], 0.0f, 5.0f);
    }
    
    PID_Enable(&daq->pid[pid_index]);
    return 0;
}

int DAQ_DisablePID(daq_state_t *daq, uint8_t pid_index)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS) return -1;
    PID_Disable(&daq->pid[pid_index]);
    DAC_SetVoltage(daq->pid_output_channel[pid_index], 0.0f);
    return 0;
}

bool DAQ_IsPIDEnabled(const daq_state_t *daq, uint8_t pid_index)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS) return false;
    return PID_IsEnabled(&daq->pid[pid_index]);
}

int DAQ_SetActiveOutputChannel(daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return -1;
    daq->active_dac_channel = channel;
    return 0;
}

uint8_t DAQ_GetActiveOutputChannel(const daq_state_t *daq)
{
    return daq ? daq->active_dac_channel : 0;
}

int DAQ_SetOutputVoltage(daq_state_t *daq, uint8_t channel, float voltage)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return -1;
    if (DAQ_IsAnyPIDDrivingDAC(daq, channel) || !daq->output_enabled[channel]) {
        return -1;
    }
    
    int ret = DAC_SetVoltage(channel, voltage);
    if (ret == 0) daq->dac_output[channel] = voltage;
    return ret;
}

int DAQ_SetOutputCurrent(daq_state_t *daq, uint8_t channel, float current_mA)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return -1;
    if (DAQ_IsAnyPIDDrivingDAC(daq, channel) || !daq->output_enabled[channel]) {
        return -1;
    }
    
    int ret = DAC_SetCurrent(channel, current_mA);
    if (ret == 0) daq->dac_output[channel] = DAC_GetVoltage(channel);
    return ret;
}

float DAQ_GetOutputVoltage(const daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return 0.0f;
    return DAC_GetVoltage(channel);
}

float DAQ_GetOutputCurrent(const daq_state_t *daq, uint8_t channel)
{
    if (!daq || channel >= DAQ_NUM_DAC_CHANNELS) return 0.0f;
    return DAC_GetCurrent(channel);
}

int DAQ_SetPIDInputChannel(daq_state_t *daq, uint8_t pid_index, uint8_t adc_channel)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS || adc_channel >= DAQ_NUM_ADC_CHANNELS) return -1;
    daq->pid_input_channel[pid_index] = adc_channel;
    return 0;
}

uint8_t DAQ_GetPIDInputChannel(const daq_state_t *daq, uint8_t pid_index)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS) return 0;
    return daq->pid_input_channel[pid_index];
}

int DAQ_SetPIDOutputChannel(daq_state_t *daq, uint8_t pid_index, uint8_t dac_channel)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS || dac_channel >= DAQ_NUM_DAC_CHANNELS) return -1;
    daq->pid_output_channel[pid_index] = dac_channel;
    return 0;
}

uint8_t DAQ_GetPIDOutputChannel(const daq_state_t *daq, uint8_t pid_index)
{
    if (!daq || pid_index >= DAQ_NUM_PID_LOOPS) return 0;
    return daq->pid_output_channel[pid_index];
}

/* ============================================================================
 * Dynamic PID Execution Loop (Handles Both Voltage and Current Modes)
 * ============================================================================ */
void DAQ_UpdatePID(daq_state_t *daq, float dt)
{
    if (!daq) return;
    
    for (uint8_t p = 0; p < DAQ_NUM_PID_LOOPS; p++) {
        if (!PID_IsEnabled(&daq->pid[p])) {
            continue;
        }
        
        uint8_t in_ch = daq->pid_input_channel[p];
        uint8_t out_ch = daq->pid_output_channel[p];
        channel_mode_t mode = daq->channels[in_ch].mode;
        
        if (mode == CHANNEL_MODE_CURRENT) {
            /* 1. Read calibrated Current in mA */
            float in_curr_mA = ADC_ReadCurrent(in_ch);
            
            /* 2. Dynamically set limits (4.0 to 20.0 mA) */
            PID_SetOutputLimits(&daq->pid[p], 4.0f, 20.0f);
            
            /* 3. Update PID */
            PID_Update(&daq->pid[p], in_curr_mA, dt);
            
            /* 4. Drive DAC using DAC_SetCurrent */
            float target_mA = PID_GetOutput(&daq->pid[p]);
            DAC_SetCurrent(out_ch, target_mA);
            daq->dac_output[out_ch] = DAC_GetVoltage(out_ch);
            
        } else {
            /* 1. Read calibrated Voltage in Volts */
            float in_volt = ADC_ReadVoltage(in_ch);
            
            /* 2. Dynamically set limits (0.0 to 5.0 V) */
            PID_SetOutputLimits(&daq->pid[p], 0.0f, 5.0f);
            
            /* 3. Update PID */
            PID_Update(&daq->pid[p], in_volt, dt);
            
            /* 4. Drive DAC using DAC_SetVoltage */
            float target_volt = PID_GetOutput(&daq->pid[p]);
            DAC_SetVoltage(out_ch, target_volt);
            daq->dac_output[out_ch] = target_volt;
        }
    }
}