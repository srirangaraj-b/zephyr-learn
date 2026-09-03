#ifndef CONTROL_PID_H
#define CONTROL_PID_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float setpoint;
    bool  use_setpoint;     /* true = Closed-Loop Setpoint Mode, false = Follower Mode */
    
    float integral;
    float prev_error;
    float output;
    
    float output_min;
    float output_max;
    
    bool enabled;
    float measurement;
    float error;
} pid_controller_t;

void PID_Init(pid_controller_t *pid);
void PID_SetKp(pid_controller_t *pid, float kp);
void PID_SetKi(pid_controller_t *pid, float ki);
void PID_SetKd(pid_controller_t *pid, float kd);
void PID_SetSetpoint(pid_controller_t *pid, float setpoint);
void PID_EnableSetpointMode(pid_controller_t *pid, bool enable);
bool PID_IsSetpointMode(const pid_controller_t *pid);

void PID_Enable(pid_controller_t *pid);
void PID_Disable(pid_controller_t *pid);
bool PID_IsEnabled(const pid_controller_t *pid);

void PID_SetOutputLimits(pid_controller_t *pid, float min_output, float max_output);
void PID_Update(pid_controller_t *pid, float measurement, float dt);

float PID_GetOutput(const pid_controller_t *pid);
float PID_GetError(const pid_controller_t *pid);
float PID_GetMeasurement(const pid_controller_t *pid);
void PID_ResetIntegral(pid_controller_t *pid);

#endif // CONTROL_PID_H