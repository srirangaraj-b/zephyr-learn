#include "scpi_commands.h"
#include "scpi_error.h"
#include "application_daq.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void SCPI_ResponseOK(char *buf, int size)
{
    if (buf && size > 0) snprintf(buf, size, "OK");
}

void SCPI_ResponseFloat(char *buf, int size, float value, int decimals)
{
    if (!buf || size <= 0) return;
    if (decimals < 0) decimals = 0;
    if (decimals > 10) decimals = 10;

    char fmt[32];
    snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
    snprintf(buf, size, fmt, (double)value);
}

void SCPI_ResponseString(char *buf, int size, const char *str)
{
    if (buf && size > 0 && str) snprintf(buf, size, "%s", str);
}

static int handle_idn(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    snprintf(response, response_size, "%s,%s,%s,%s", daq->manufacturer, daq->model, daq->serial, daq->version);
    return 0;
}

static int handle_rst(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    DAQ_Init(daq);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_cls(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    (void)daq;
    if (cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    SCPI_Error_Clear();
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_opc(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    (void)daq;
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    snprintf(response, response_size, "1");
    return 0;
}

static int handle_conf_volt(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query || !cmd->has_channel || cmd->channel >= DAQ_NUM_ADC_CHANNELS) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    DAQ_ConfigChannel_Voltage(daq, cmd->channel);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_conf_curr(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query || !cmd->has_channel || cmd->channel >= DAQ_NUM_ADC_CHANNELS) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    DAQ_ConfigChannel_Current(daq, cmd->channel);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_meas_volt(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query || !cmd->has_channel || cmd->channel >= DAQ_NUM_ADC_CHANNELS) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    float voltage = DAQ_ReadChannel(daq, cmd->channel);
    SCPI_ResponseFloat(response, response_size, voltage, 4);
    return 0;
}

static int handle_meas_curr(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query || !cmd->has_channel || cmd->channel >= DAQ_NUM_ADC_CHANNELS) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    float current = DAQ_ReadChannel(daq, cmd->channel);
    SCPI_ResponseFloat(response, response_size, current, 4);
    return 0;
}

static int handle_meas_volt_all(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    char buf[32];
    response[0] = '\0';
    for (uint8_t ch = 0; ch < DAQ_NUM_ADC_CHANNELS; ch++) {
        float val = (DAQ_GetChannelMode(daq, ch) == CHANNEL_MODE_VOLTAGE) ? DAQ_ReadChannel(daq, ch) : 0.0f;
        snprintf(buf, sizeof(buf), "%.4f", (double)val);
        if (ch > 0) strncat(response, ",", response_size - strlen(response) - 1);
        strncat(response, buf, response_size - strlen(response) - 1);
    }
    return 0;
}

static int handle_meas_curr_all(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    char buf[32];
    response[0] = '\0';
    for (uint8_t ch = 0; ch < 2; ch++) {
        float val = (DAQ_GetChannelMode(daq, ch) == CHANNEL_MODE_CURRENT) ? DAQ_ReadChannel(daq, ch) : 0.0f;
        snprintf(buf, sizeof(buf), "%.4f", (double)val);
        if (ch > 0) strncat(response, ",", response_size - strlen(response) - 1);
        strncat(response, buf, response_size - strlen(response) - 1);
    }
    return 0;
}

static int handle_samp_rate(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) {
        snprintf(response, response_size, "%u", DAQ_GetSampleRate(daq));
        return 0;
    }
    if (cmd->param_count < 1 || cmd->params[0] < 1.0f) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    DAQ_SetSampleRate(daq, (uint32_t)cmd->params[0]);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_samp_count(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) {
        snprintf(response, response_size, "%u", DAQ_GetSampleCount(daq));
        return 0;
    }
    if (cmd->param_count < 1 || cmd->params[0] < 1.0f) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    DAQ_SetSampleCount(daq, (uint32_t)cmd->params[0]);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_sour_chan(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) {
        snprintf(response, response_size, "%u", DAQ_GetActiveOutputChannel(daq));
        return 0;
    }
    if (cmd->param_count < 1 || cmd->params[0] < 0 || cmd->params[0] >= DAQ_NUM_DAC_CHANNELS) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    DAQ_SetActiveOutputChannel(daq, (uint8_t)cmd->params[0]);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_sour_volt(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t ch = cmd->has_channel ? cmd->channel : DAQ_GetActiveOutputChannel(daq);
    if (ch >= DAQ_NUM_DAC_CHANNELS) { SCPI_Error_Add(-222, "Data out of range"); return -1; }

    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, DAQ_GetOutputVoltage(daq, ch), 3);
        return 0;
    }

    float target_volt = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    if (target_volt < 0.0f || target_volt > 5.0f) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    if (DAQ_SetOutputVoltage(daq, ch, target_volt) < 0) {
        SCPI_Error_Add(-221, "Settings conflict");
        return -1;
    }
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_sour_curr(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t ch = cmd->has_channel ? cmd->channel : DAQ_GetActiveOutputChannel(daq);
    if (ch >= DAQ_NUM_DAC_CHANNELS) { SCPI_Error_Add(-222, "Data out of range"); return -1; }

    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, DAQ_GetOutputCurrent(daq, ch), 3);
        return 0;
    }

    float target_curr = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    if (target_curr < 4.0f || target_curr > 20.0f) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    if (DAQ_SetOutputCurrent(daq, ch, target_curr) < 0) {
        SCPI_Error_Add(-221, "Settings conflict");
        return -1;
    }
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_outp(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t ch = cmd->has_channel ? cmd->channel : DAQ_GetActiveOutputChannel(daq);
    if (ch >= DAQ_NUM_DAC_CHANNELS) { SCPI_Error_Add(-222, "Data out of range"); return -1; }

    if (cmd->is_query) {
        snprintf(response, response_size, "%d", DAQ_IsOutputEnabled(daq, ch) ? 1 : 0);
        return 0;
    }

    char upper[64];
    strncpy(upper, cmd->raw_params, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';
    for (int i = 0; upper[i]; i++) upper[i] = toupper((unsigned char)upper[i]);

    if (strstr(upper, "ON") || strstr(upper, " 1") || strcmp(upper, "1") == 0) {
        DAQ_EnableOutput(daq, ch);
        SCPI_ResponseOK(response, response_size);
        return 0;
    } else if (strstr(upper, "OFF") || strstr(upper, " 0") || strcmp(upper, "0") == 0) {
        DAQ_DisableOutput(daq, ch);
        SCPI_ResponseOK(response, response_size);
        return 0;
    }
    SCPI_Error_Add(-222, "Data out of range");
    return -1;
}

static inline uint8_t get_pid_index(const scpi_command_t *cmd)
{
    if (cmd && cmd->has_channel && cmd->channel < DAQ_NUM_PID_LOOPS) {
        return cmd->channel;
    }
    return 0;
}

static int handle_pid_in_chan(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        snprintf(response, response_size, "%u", DAQ_GetPIDInputChannel(daq, p));
        return 0;
    }
    float adc_ch = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    if (DAQ_SetPIDInputChannel(daq, p, (uint8_t)adc_ch) < 0) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_out_chan(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        snprintf(response, response_size, "%u", DAQ_GetPIDOutputChannel(daq, p));
        return 0;
    }
    float dac_ch = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    if (DAQ_SetPIDOutputChannel(daq, p, (uint8_t)dac_ch) < 0) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_kp(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, daq->pid[p].kp, 4);
        return 0;
    }
    float kp = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    PID_SetKp(&daq->pid[p], kp);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_ki(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, daq->pid[p].ki, 4);
        return 0;
    }
    float ki = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    PID_SetKi(&daq->pid[p], ki);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_kd(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, daq->pid[p].kd, 4);
        return 0;
    }
    float kd = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    PID_SetKd(&daq->pid[p], kd);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_set(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        SCPI_ResponseFloat(response, response_size, daq->pid[p].setpoint, 3);
        return 0;
    }
    float sp = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];
    PID_SetSetpoint(&daq->pid[p], sp);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_on(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    DAQ_EnablePID(daq, p);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_off(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    DAQ_DisablePID(daq, p);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

static int handle_pid_stat(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    snprintf(response, response_size, "%d", DAQ_IsPIDEnabled(daq, p) ? 1 : 0);
    return 0;
}

static int handle_pid_meas(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    SCPI_ResponseFloat(response, response_size, PID_GetMeasurement(&daq->pid[p]), 3);
    return 0;
}

static int handle_pid_err(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    SCPI_ResponseFloat(response, response_size, PID_GetError(&daq->pid[p]), 3);
    return 0;
}

static int handle_pid_out(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    uint8_t p = get_pid_index(cmd);
    SCPI_ResponseFloat(response, response_size, PID_GetOutput(&daq->pid[p]), 3);
    return 0;
}

static int handle_syst_err(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    (void)daq;
    if (!cmd->is_query) { SCPI_Error_Add(-113, "Undefined header"); return -1; }
    scpi_error_t error;
    SCPI_Error_Get(&error);
    SCPI_Error_Format(&error, response, response_size);
    return 0;
}
/* In src/scpi_commands.c, add this handler: */

static int handle_pid_set_mode(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    if (cmd->is_query) {
        snprintf(response, response_size, "%d", PID_IsSetpointMode(&daq->pid[p]) ? 1 : 0);
        return 0;
    }
    
    char upper[64];
    strncpy(upper, cmd->raw_params, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';
    for (int i = 0; upper[i]; i++) upper[i] = toupper((unsigned char)upper[i]);
    
    if (strstr(upper, "ON") || strstr(upper, " 1") || strcmp(upper, "1") == 0) {
        PID_EnableSetpointMode(&daq->pid[p], true);
        SCPI_ResponseOK(response, response_size);
        return 0;
    } else if (strstr(upper, "OFF") || strstr(upper, " 0") || strcmp(upper, "0") == 0) {
        PID_EnableSetpointMode(&daq->pid[p], false);
        SCPI_ResponseOK(response, response_size);
        return 0;
    }
    
    SCPI_Error_Add(-222, "Data out of range");
    return -1;
}

/* ====================================================================
   Handler: PID:SET:PERC and PID:SET:PERC?
   ==================================================================== */
static int handle_pid_set_perc(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    uint8_t p = get_pid_index(cmd);
    uint8_t in_ch = daq->pid_input_channel[p];
    channel_mode_t mode = daq->channels[in_ch].mode;

    /* --- QUERY: PID:SET:PERC? (@p) --- */
    if (cmd->is_query) {
        float current_sp = daq->pid[p].setpoint;
        float percentage = 0.0f;

        if (mode == CHANNEL_MODE_CURRENT) {
            /* 4-20 mA span (16 mA range) */
            percentage = ((current_sp - 4.0f) / 16.0f) * 100.0f;
        } else {
            /* 0-5 V span (5 V range) */
            percentage = (current_sp / 5.0f) * 100.0f;
        }

        if (percentage < 0.0f) percentage = 0.0f;
        if (percentage > 100.0f) percentage = 100.0f;

        SCPI_ResponseFloat(response, response_size, percentage, 2);
        return 0;
    }

    /* --- COMMAND: PID:SET:PERC (@p) <val> --- */
    float perc_val = cmd->has_channel ? ((cmd->param_count >= 2) ? cmd->params[1] : cmd->params[0]) : cmd->params[0];

    if (perc_val < 0.0f || perc_val > 100.0f) {
        SCPI_Error_Add(-222, "Data out of range");
        return -1;
    }

    float target_sp = 0.0f;
    if (mode == CHANNEL_MODE_CURRENT) {
        /* Map 0-100% -> 4.0 - 20.0 mA */
        target_sp = 4.0f + ((perc_val / 100.0f) * 16.0f);
    } else {
        /* Map 0-100% -> 0.0 - 5.0 V */
        target_sp = (perc_val / 100.0f) * 5.0f;
    }

    PID_SetSetpoint(&daq->pid[p], target_sp);
    SCPI_ResponseOK(response, response_size);
    return 0;
}

int SCPI_ExecuteCommand(const scpi_command_t *cmd, daq_state_t *daq, char *response, int response_size)
{
    if (!cmd || !daq || !response || cmd->token_count == 0) return -1;

    if (SCPI_Match(cmd, "*IDN", NULL)) return handle_idn(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "*RST", NULL)) return handle_rst(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "*CLS", NULL)) return handle_cls(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "*OPC", NULL)) return handle_opc(cmd, daq, response, response_size);

    if (SCPI_Match(cmd, "CONF", "VOLT", NULL)) return handle_conf_volt(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "CONF", "CURR", NULL)) return handle_conf_curr(cmd, daq, response, response_size);

    if (SCPI_Match(cmd, "MEAS", "VOLT", "ALL", NULL)) return handle_meas_volt_all(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "MEAS", "CURR", "ALL", NULL)) return handle_meas_curr_all(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "MEAS", "VOLT", NULL)) return handle_meas_volt(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "MEAS", "CURR", NULL)) return handle_meas_curr(cmd, daq, response, response_size);

    if (SCPI_Match(cmd, "SAMP", "RATE", NULL)) return handle_samp_rate(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "SAMP", "COUNT", NULL)) return handle_samp_count(cmd, daq, response, response_size);

    if (SCPI_Match(cmd, "SOUR", "CHAN", NULL)) return handle_sour_chan(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "SOUR", "VOLT", NULL)) return handle_sour_volt(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "SOUR", "CURR", NULL)) return handle_sour_curr(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "OUTP", NULL)) return handle_outp(cmd, daq, response, response_size);

    if (SCPI_Match(cmd, "PID", "IN", "CHAN", NULL)) return handle_pid_in_chan(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "OUT", "CHAN", NULL)) return handle_pid_out_chan(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "KP", NULL)) return handle_pid_kp(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "KI", NULL)) return handle_pid_ki(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "KD", NULL)) return handle_pid_kd(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "SET", NULL)) return handle_pid_set(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "ON", NULL)) return handle_pid_on(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "OFF", NULL)) return handle_pid_off(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "STAT", NULL)) return handle_pid_stat(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "MEAS", NULL)) return handle_pid_meas(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "ERR", NULL)) return handle_pid_err(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "OUT", NULL)) return handle_pid_out(cmd, daq, response, response_size);
    if (SCPI_Match(cmd, "PID", "SET", "MODE", NULL)) return handle_pid_set_mode(cmd, daq, response, response_size);
if (SCPI_Match(cmd, "PID", "SET", "PERC", NULL)) return handle_pid_set_perc(cmd, daq, response, response_size);
    
    if (SCPI_Match(cmd, "PID", "SET", NULL)) return handle_pid_set(cmd, daq, response, response_size);
    
    if (SCPI_Match(cmd, "SYST", "ERR", NULL)) return handle_syst_err(cmd, daq, response, response_size);

    SCPI_Error_Add(-113, "Undefined header");
    return -1;
}