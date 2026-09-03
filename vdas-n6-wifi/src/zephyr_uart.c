/**
 * Zephyr UART Driver for SCPI DAQ
 *
 * Wraps Zephyr's UART device API to expose two independent physical
 * ports through the same interface the application already uses:
 *
 *   UART_PORT_CONSOLE -> ST-Link VCOM console (PB14 TX / PB15 RX, 115200 8N1)
 *   UART_PORT_USART3  -> second external UART link (USART3)
 *
 * Each port gets its own RX ring buffer, filled by its own interrupt
 * handler, so the two links are fully independent from the ISR level
 * up through UART_GetChar(). Nothing above this file needs to know
 * there are two physical UARTs beyond passing the right port enum.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "drivers_uart.h"

#define UART_RX_BUF_SIZE 1024

typedef struct {
    const struct device *dev;
    uint8_t rx_buf[UART_RX_BUF_SIZE];
    struct ring_buf rx_ring;
} uart_port_ctx_t;

static uart_port_ctx_t s_ports[UART_PORT_COUNT];

/**
 * Shared UART interrupt handler - called for whichever port fires.
 * user_data tells us which port's context/ring buffer to use, so one
 * handler function serves both physical UARTs.
 */
static void uart_isr_handler(const struct device *dev, void *user_data)
{
    uart_port_ctx_t *ctx = (uart_port_ctx_t *)user_data;
    uint8_t c;

    uart_irq_update(dev);

    if (!uart_irq_rx_ready(dev)) {
        return;
    }

    /* Read all available characters */
    while (uart_poll_in(dev, &c) == 0) {
        uint32_t ret = ring_buf_put(&ctx->rx_ring, &c, 1);
        if (ret < 1) {
            /* Buffer full, character dropped */
            /* In production, could log this */
        }
    }
}

static void init_one_port(uart_port_t port, const struct device *dev, const char *label)
{
    uart_port_ctx_t *ctx = &s_ports[port];

    ctx->dev = dev;

    if (!device_is_ready(ctx->dev)) {
        printk("UART port '%s' not ready!\n", label);
        ctx->dev = NULL;
        return;
    }

    ring_buf_init(&ctx->rx_ring, sizeof(ctx->rx_buf), ctx->rx_buf);

    uart_irq_callback_user_data_set(ctx->dev, uart_isr_handler, ctx);
    uart_irq_rx_enable(ctx->dev);

    printk("UART port '%s' initialized: %s\n", label, ctx->dev->name);
}

/**
 * Initialize both UART ports.
 */
void UART_Init(void)
{
    init_one_port(UART_PORT_CONSOLE,
                   DEVICE_DT_GET(DT_CHOSEN(zephyr_console)),
                   "console");

#if DT_NODE_EXISTS(DT_NODELABEL(usart3))
    init_one_port(UART_PORT_USART3,
                   DEVICE_DT_GET(DT_NODELABEL(usart3)),
                   "usart3");
#else
    printk("USART3 devicetree node not found - is it enabled in your overlay?\n");
    s_ports[UART_PORT_USART3].dev = NULL;
#endif
}

/**
 * Send a single character via the given UART port (blocking).
 */
void UART_SendChar(uart_port_t port, char c)
{
    if (port >= UART_PORT_COUNT || !s_ports[port].dev) {
        return;
    }
    uart_poll_out(s_ports[port].dev, (uint8_t)c);
}

/**
 * Send a null-terminated string via the given UART port.
 */
void UART_Send(uart_port_t port, const char *str)
{
    if (!str || port >= UART_PORT_COUNT || !s_ports[port].dev) {
        return;
    }

    while (*str) {
        UART_SendChar(port, *str++);
    }
}

/**
 * Send formatted output (printf-style) via the given UART port.
 */
void UART_Printf(uart_port_t port, const char *format, ...)
{
    char buffer[256];
    va_list args;

    if (port >= UART_PORT_COUNT || !s_ports[port].dev) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    UART_Send(port, buffer);
}

/**
 * Read a character from the given UART port (non-blocking).
 */
bool UART_GetChar(uart_port_t port, uint8_t *c)
{
    if (!c || port >= UART_PORT_COUNT || !s_ports[port].dev) {
        return false;
    }

    size_t bytes_read = ring_buf_get(&s_ports[port].rx_ring, c, 1);
    return (bytes_read > 0);
}

/**
 * Get UART device pointer for a port (for direct Zephyr API access if needed)
 */
const struct device *UART_GetDevice(uart_port_t port)
{
    if (port >= UART_PORT_COUNT) {
        return NULL;
    }
    return s_ports[port].dev;
}