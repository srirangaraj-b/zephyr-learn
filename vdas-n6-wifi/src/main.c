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

/* Per-port command line buffer/state. Each UART port (console, USART3)
   assembles its own command line independently, so a half-typed line
   on one link can never bleed into the other. */
#define COMMAND_BUFFER_SIZE 256

typedef struct {
    char buffer[COMMAND_BUFFER_SIZE];
    int  pos;
} cmd_line_state_t;

static cmd_line_state_t g_cmd_state[UART_PORT_COUNT];

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
static void service_uart_port(uart_port_t port);
static void process_command_line(uart_port_t port, const char *line);
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

   Services BOTH UART ports (console + USART3) every tick. Each port
   is fully independent: its own RX ring buffer (filled by its own
   ISR), its own in-progress command-line buffer, and any response is
   written back out that same port.
   ==================================================================== */

static void service_uart_port(uart_port_t port)
{
    uint8_t uart_char;
    cmd_line_state_t *st = &g_cmd_state[port];

    /* Drain everything currently queued for this port */
    while (UART_GetChar(port, &uart_char)) {
        if (uart_char == '\r' || uart_char == '\n') {
            /* Command complete - process it.
               NOTE: deliberately no newline sent here. The characters
               already echoed above form "COMMAND", and
               process_command_line() appends "->(response)\n" right
               after it with no line break in between, so the whole
               transaction is exactly one line:
                   COMMAND->(response)\n
               Sending a newline here would split it into two lines
               and desync any host parser expecting one line per
               command (this was the actual bug: it caused every
               later line to shift by one). */
            if (st->pos > 0) {
                st->buffer[st->pos] = '\0';
                process_command_line(port, st->buffer);
                st->pos = 0;
            }
        } else if (uart_char == '\b' || uart_char == 127) {
            /* Backspace */
            if (st->pos > 0) {
                st->pos--;
                UART_Send(port, "\b \b");
            }
        } else if (uart_char >= 32 && uart_char < 127) {
            /* Printable character - add to buffer and echo back on
               the SAME port it arrived on (printk()/putchar() would
               only ever reach the console, never USART3) */
            if (st->pos < COMMAND_BUFFER_SIZE - 1) {
                st->buffer[st->pos++] = uart_char;
                UART_SendChar(port, uart_char);
            }
        }
    }
}

static void uart_poll_thread(void *arg1, void *arg2, void *arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    
    printk("[UART] Thread started\n");
    
    while (1) {
        service_uart_port(UART_PORT_CONSOLE);
        service_uart_port(UART_PORT_USART3);

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
   Command Processing with Formatted Delimiter: Command->(Response)

   `port` identifies which physical UART the command came in on, so
   the response is written back out that same link.
   ==================================================================== */

static void process_command_line(uart_port_t port, const char *line)
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
        UART_Send(port, formatted_out);
        return;
    }
    
    /* Execute the command */
    memset(response_buffer, 0, sizeof(response_buffer));
    int cmd_ret = SCPI_ExecuteCommand(&cmd, &g_daq, response_buffer, sizeof(response_buffer));
    
    if (cmd_ret == 0) {
        /* Successful execution */
        snprintf(formatted_out, sizeof(formatted_out), "->(%s)\n", response_buffer);
        UART_Send(port, formatted_out);
    } else {
        /* Error occurred */
        if (SCPI_Error_Get(&g_last_error)) {
            char err_buf[128];
            SCPI_Error_Format(&g_last_error, err_buf, sizeof(err_buf));
            snprintf(formatted_out, sizeof(formatted_out), "->(%s)\n", err_buf);
            UART_Send(port, formatted_out);
        } else {
            snprintf(formatted_out, sizeof(formatted_out), "->(ERROR)\n");
            UART_Send(port, formatted_out);
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
    /* Initialize system */
    system_init();
    
    /* Main thread is now ready to process user input */

    
    /* Main thread can be used for other tasks if needed */
    /* For now, just keep it alive */
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}