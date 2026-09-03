/**
 * Zephyr UART Driver for SCPI DAQ
 *
 * This driver wraps Zephyr's UART device API to provide
 * the same interface as the mock driver.
 *
 * Hardware: NUCLEO-N657X0-Q (STM32N657)
 *          UART1 via ST-Link (PB14 TX, PB15 RX)
 *          115200 baud, 8N1
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* Get console device (UART for serial console) */
static const struct device *uart_dev = NULL;

/* Ring buffer for received characters (non-blocking queue) */
#define UART_RX_BUF_SIZE 1024
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];
static struct ring_buf uart_rx_ring;
    
/**
 * UART interrupt handler - called when character received
 * Adds received character to ring buffer
 */
static void uart_isr_handler(const struct device *dev, void *ctx)
{
    uint8_t c;
    
    /* Read all available characters */
    while (uart_poll_in(dev, &c) == 0) {
        uint32_t dropped;
        
        /* Try to put character in ring buffer */
        uint32_t ret = ring_buf_put(&uart_rx_ring, &c, 1);
        if (ret < 1) {
            /* Buffer full, character dropped */
            /* In production, could log this */
        }
    }
}

/**
 * Initialize UART subsystem
 */
void UART_Init(void)
{
    /* Get the console device (typically UART1 on NUCLEO) */
    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready!\n");
        return;
    }
    
    /* Initialize ring buffer */
    ring_buf_init(&uart_rx_ring, sizeof(uart_rx_buf), uart_rx_buf);
    
    /* Configure interrupt handler for received data */
    uart_irq_callback_set(uart_dev, uart_isr_handler);
    uart_irq_rx_enable(uart_dev);
    
    printk("UART initialized: %s\n", uart_dev->name);
}

/**
 * Send a single character via UART
 *
 * Blocking operation - waits for UART ready
 */
void UART_SendChar(char c)
{
    if (uart_dev) {
        uart_poll_out(uart_dev, (uint8_t)c);
    }
}

/**
 * Send a null-terminated string via UART
 */
void UART_Send(const char *str)
{
    if (!str || !uart_dev) {
        return;
    }
    
    while (*str) {
        UART_SendChar(*str++);
    }
}

/**
 * Send formatted output (printf-style)
 */
void UART_Printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    
    if (!uart_dev) {
        return;
    }
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    UART_Send(buffer);
}

/**
 * Read a character from UART (non-blocking)
 *
 * Returns true if character available, false otherwise
 * Character stored in *c
 */
bool UART_GetChar(uint8_t *c)
{
    if (!c || !uart_dev) {
        return false;
    }
    
    /* Try to get character from ring buffer */
    size_t bytes_read = ring_buf_get(&uart_rx_ring, c, 1);
    
    return (bytes_read > 0);
}

/**
 * Get UART device pointer (for direct Zephyr API access if needed)
 */
const struct device *UART_GetDevice(void)
{
    return uart_dev;
}
