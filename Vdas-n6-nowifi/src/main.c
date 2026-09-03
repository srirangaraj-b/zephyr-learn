#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Include application headers */
#include "drivers_uart.h"
#include "drivers_adc.h"
#include "drivers_dac.h"
#include "scpi_parser.h"
#include "scpi_commands.h"
#include "scpi_error.h"
#include "control_pid.h"
#include "application_daq.h"

/* ====================================================================
   Global Application State
   ==================================================================== */

static daq_state_t g_daq;
static scpi_error_t g_last_error;

/* Command line buffer */
#define COMMAND_BUFFER_SIZE 256
static char command_buffer[COMMAND_BUFFER_SIZE];
static int command_pos = 0;

/* Timing and synchronization */
#define PID_UPDATE_PERIOD_MS 10
#define UART_POLL_INTERVAL_MS 1

/* Thread stacks */
#define UART_THREAD_STACK_SIZE 4096
#define PID_THREAD_STACK_SIZE 2048

/* Semaphores and timers */
static struct k_timer pid_timer;
static struct k_sem pid_update_sem;

/* ====================================================================
   Forward Declarations
   ==================================================================== */

static void uart_poll_thread(void *arg1, void *arg2, void *arg3);
static void pid_update_thread(void *arg1, void *arg2, void *arg3);
static void pid_timer_expiry(struct k_timer *timer);
static void process_command_line(const char *line);
static void system_init(void);

/* ====================================================================
   Thread Definitions
   ==================================================================== */

/* Make UART thread higher priority so it drains incoming bytes immediately */
K_THREAD_DEFINE(uart_thread, UART_THREAD_STACK_SIZE,
                uart_poll_thread, NULL, NULL, NULL,
                1, 0, 0);  /* Priority 1 (Higher Priority) */

K_THREAD_DEFINE(pid_thread, PID_THREAD_STACK_SIZE,
                pid_update_thread, NULL, NULL, NULL,
                3, 0, 0);  /* Priority 3 */

/* ====================================================================
   PID Timer Expiry Handler
   
   Called every PID_UPDATE_PERIOD_MS to trigger PID update
   ==================================================================== */

static void pid_timer_expiry(struct k_timer *timer)
{
    /* Signal PID thread to wake up */
    k_sem_give(&pid_update_sem);
}

/* ====================================================================
   UART Poll Thread
   
   Polls UART for incoming characters and processes commands
   ==================================================================== */

static void uart_poll_thread(void *arg1, void *arg2, void *arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    
    printk("[UART] Thread started\n");
    
    while (1) {
        uint8_t uart_char;
        
        /* Poll UART (non-blocking) */
        if (UART_GetChar(&uart_char)) {
            if (uart_char == '\r' || uart_char == '\n') {
                /* Command complete - process it */
                if (command_pos > 0) {
                    command_buffer[command_pos] = '\0';
                    printk("\n");
                    process_command_line(command_buffer);
                    command_pos = 0;
                }
            } else if (uart_char == '\b' || uart_char == 127) {
                /* Backspace */
                if (command_pos > 0) {
                    command_pos--;
                    printk("\b \b");
                }
            } else if (uart_char >= 32 && uart_char < 127) {
                /* Printable character - add to buffer and echo */
                if (command_pos < COMMAND_BUFFER_SIZE - 1) {
                    command_buffer[command_pos++] = uart_char;
                    putchar(uart_char);
                }
            }
        }
        
        /* Yield to prevent starving other threads */
        k_sleep(K_MSEC(UART_POLL_INTERVAL_MS));
    }
}

/* ====================================================================
   PID Update Thread
   
   Updates PID controller at fixed 10ms interval
   ==================================================================== */

static void pid_update_thread(void *arg1, void *arg2, void *arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    
    printk("[PID] Thread started\n");
    
    while (1) {
        /* Wait for PID timer to expire */
        k_sem_take(&pid_update_sem, K_FOREVER);
        
        /* Update PID with fixed time step */
        float dt = (float)PID_UPDATE_PERIOD_MS / 1000.0f;
        DAQ_UpdatePID(&g_daq, dt);
    }
}

/* ====================================================================
   Command Processing
   ==================================================================== */

/* ====================================================================
   Command Processing with Formatted Delimiter: Command->(Response)
   ==================================================================== */

static void process_command_line(const char *line)
{
    char response_buffer[256];
    char formatted_out[512];
    scpi_command_t cmd;
    
    if (!line || line[0] == '\0') {
        return;  /* Empty line, ignore */
    }
    
    /* Parse the command */
    int parse_ret = SCPI_Parse(line, &cmd);
    if (parse_ret < 0) {
        SCPI_Error_Add(-113, "Undefined header");
        snprintf(formatted_out, sizeof(formatted_out), "->(-113,\"Undefined header\")\n");
        UART_Send(formatted_out);
        return;
    }
    
    /* Execute the command */
    memset(response_buffer, 0, sizeof(response_buffer));
    int cmd_ret = SCPI_ExecuteCommand(&cmd, &g_daq, response_buffer, sizeof(response_buffer));
    
    if (cmd_ret == 0) {
        /* Successful execution */
        snprintf(formatted_out, sizeof(formatted_out), "->(%s)\n", response_buffer);
        UART_Send(formatted_out);
    } else {
        /* Error occurred */
        if (SCPI_Error_Get(&g_last_error)) {
            char err_buf[128];
            SCPI_Error_Format(&g_last_error, err_buf, sizeof(err_buf));
            snprintf(formatted_out, sizeof(formatted_out), "->(%s)\n", err_buf);
            UART_Send(formatted_out);
        } else {
            snprintf(formatted_out, sizeof(formatted_out), "->(ERROR)\n");
            UART_Send(formatted_out);
        }
    }
}

/* ====================================================================
   System Initialization
   ==================================================================== */

static void system_init(void)
{

    UART_Init();

    ADC_Init();

    DAC_Init();

    SCPI_Error_Init();

    DAQ_Init(&g_daq);

k_sem_init(&pid_update_sem, 0, 1);

k_timer_init(&pid_timer, pid_timer_expiry, NULL);

k_timer_start(&pid_timer,
              K_MSEC(PID_UPDATE_PERIOD_MS),
              K_MSEC(PID_UPDATE_PERIOD_MS));
    printk("\n");
    printk("========================================\n");
    printk("System initialized successfully\n");
    printk("========================================\n\n");
    
}

/* ====================================================================
   Main Entry Point
   ==================================================================== */

int main(void)
{
    system_init();
    
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}
        