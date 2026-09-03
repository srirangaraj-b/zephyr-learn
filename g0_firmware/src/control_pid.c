#include "control_pid.h"

void PID_Init(pid_controller_t *pid)
{
    if (!pid) return;
    
    pid->kp = 1.0f;
    pid->ki = 0.0f;
    pid->kd = 0.0f;
    pid->setpoint = 0.0f;
    pid->use_setpoint = false;
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output = 0.0f;
    
    pid->output_min = 0.0f;
    pid->output_max = 20.0f;
    
    pid->enabled = false;
    pid->measurement = 0.0f;
    pid->error = 0.0f;
}

void PID_SetKp(pid_controller_t *pid, float kp) { if (pid) pid->kp = kp; }
void PID_SetKi(pid_controller_t *pid, float ki) { if (pid) pid->ki = ki; }
void PID_SetKd(pid_controller_t *pid, float kd) { if (pid) pid->kd = kd; }
void PID_SetSetpoint(pid_controller_t *pid, float setpoint) { if (pid) pid->setpoint = setpoint; }

void PID_EnableSetpointMode(pid_controller_t *pid, bool enable)
{
    if (!pid) return;
    pid->use_setpoint = enable;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}

bool PID_IsSetpointMode(const pid_controller_t *pid)
{
    return pid ? pid->use_setpoint : false;
}

void PID_Enable(pid_controller_t *pid)
{
    if (!pid) return;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->enabled = true;
}

void PID_Disable(pid_controller_t *pid)
{
    if (!pid) return;
    pid->enabled = false;
    pid->output = 0.0f;
}

bool PID_IsEnabled(const pid_controller_t *pid) { return pid ? pid->enabled : false; }

void PID_SetOutputLimits(pid_controller_t *pid, float min_output, float max_output)
{
    if (!pid) return;
    pid->output_min = min_output;
    pid->output_max = max_output;
}

void PID_Update(pid_controller_t *pid, float measurement, float dt)
{
    if (!pid || !pid->enabled) return;
    
    pid->measurement = measurement;
    float output = 0.0f;
    
    if (pid->use_setpoint) {
        /* Closed-Loop Setpoint Regulation */
        float error = pid->setpoint - measurement;
        pid->error = error;
        
        float p_term = pid->kp * error;
        
        if (dt > 0.0f && pid->ki > 0.0f) {
            pid->integral += pid->ki * error * dt;
            if (pid->integral > pid->output_max) pid->integral = pid->output_max;
            if (pid->integral < pid->output_min) pid->integral = pid->output_min;
        } else {
            pid->integral = 0.0f;
        }
        
        float d_term = 0.0f;
        if (dt > 0.0f && pid->kd > 0.0f) {
            d_term = pid->kd * (error - pid->prev_error) / dt;
        }
        pid->prev_error = error;
        
        output = p_term + pid->integral + d_term;
        
    } else {
        /* Passthrough / Follower Mode */
        pid->error = 0.0f;
        output = pid->kp * measurement;
    }
    
    /* Clamp to configured min/max */
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;
    
    pid->output = output;
}

float PID_GetOutput(const pid_controller_t *pid) { return pid ? pid->output : 0.0f; }
float PID_GetError(const pid_controller_t *pid) { return pid ? pid->error : 0.0f; }
float PID_GetMeasurement(const pid_controller_t *pid) { return pid ? pid->measurement : 0.0f; }

void PID_ResetIntegral(pid_controller_t *pid)
{
    if (!pid) return;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}