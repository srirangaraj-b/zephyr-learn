#ifndef APPLICATION_DAQ_H
#define APPLICATION_DAQ_H

#include <stdint.h>
#include <stdbool.h>
#include "control_pid.h"
#include "calibration_config.h"

#ifndef DAQ_NUM_PID_LOOPS
#define DAQ_NUM_PID_LOOPS 2
#endif

typedef enum {
    CHANNEL_MODE_VOLTAGE = 0,
    CHANNEL_MODE_CURRENT = 1,
} channel_mode_t;

typedef struct {
    channel_mode_t mode;
    float last_measurement;
} channel_config_t;

typedef struct {
    char manufacturer[32];
    char model[32];
    char serial[32];
    char version[32];
    
    channel_config_t channels[DAQ_NUM_ADC_CHANNELS];
    
    uint32_t sample_rate;
    uint32_t sample_count;
    
    bool output_enabled[DAQ_NUM_DAC_CHANNELS];
    float dac_output[DAQ_NUM_DAC_CHANNELS];
    uint8_t active_dac_channel;
    
    /* Dual PID Controller State */
    uint8_t pid_input_channel[DAQ_NUM_PID_LOOPS];
    uint8_t pid_output_channel[DAQ_NUM_PID_LOOPS];
    pid_controller_t pid[DAQ_NUM_PID_LOOPS];
} daq_state_t;

void DAQ_Init(daq_state_t *daq);

int DAQ_ConfigChannel_Voltage(daq_state_t *daq, uint8_t channel);
int DAQ_ConfigChannel_Current(daq_state_t *daq, uint8_t channel);
channel_mode_t DAQ_GetChannelMode(const daq_state_t *daq, uint8_t channel);

float DAQ_ReadChannel(daq_state_t *daq, uint8_t channel);
void DAQ_SetSampleRate(daq_state_t *daq, uint32_t rate);
uint32_t DAQ_GetSampleRate(const daq_state_t *daq);
void DAQ_SetSampleCount(daq_state_t *daq, uint32_t count);
uint32_t DAQ_GetSampleCount(const daq_state_t *daq);

/* Output Controls */
void DAQ_EnableOutput(daq_state_t *daq, uint8_t channel);
void DAQ_DisableOutput(daq_state_t *daq, uint8_t channel);
bool DAQ_IsOutputEnabled(const daq_state_t *daq, uint8_t channel);

int DAQ_SetActiveOutputChannel(daq_state_t *daq, uint8_t channel);
uint8_t DAQ_GetActiveOutputChannel(const daq_state_t *daq);

int DAQ_SetOutputVoltage(daq_state_t *daq, uint8_t channel, float voltage);
int DAQ_SetOutputCurrent(daq_state_t *daq, uint8_t channel, float current_mA);
float DAQ_GetOutputVoltage(const daq_state_t *daq, uint8_t channel);
float DAQ_GetOutputCurrent(const daq_state_t *daq, uint8_t channel);

/* Dual PID Controls & Routing */
int DAQ_EnablePID(daq_state_t *daq, uint8_t pid_index);
int DAQ_DisablePID(daq_state_t *daq, uint8_t pid_index);
bool DAQ_IsPIDEnabled(const daq_state_t *daq, uint8_t pid_index);
bool DAQ_IsAnyPIDDrivingDAC(const daq_state_t *daq, uint8_t dac_channel);

int DAQ_SetPIDInputChannel(daq_state_t *daq, uint8_t pid_index, uint8_t adc_channel);
uint8_t DAQ_GetPIDInputChannel(const daq_state_t *daq, uint8_t pid_index);
int DAQ_SetPIDOutputChannel(daq_state_t *daq, uint8_t pid_index, uint8_t dac_channel);
uint8_t DAQ_GetPIDOutputChannel(const daq_state_t *daq, uint8_t pid_index);

void DAQ_UpdatePID(daq_state_t *daq, float dt);

#endif // APPLICATION_DAQ_H