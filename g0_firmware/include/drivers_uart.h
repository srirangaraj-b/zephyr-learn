#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

/**
 * UART Driver Module
 *
 * Provides simple UART I/O functions, now multiplexed across two
 * independent physical ports:
 *
 *   UART_PORT_CONSOLE - ST-Link VCOM (the original "serial monitor" link)
 *   UART_PORT_USART1  - device-to-device UART link (USART1)
 *
 * Both ports run the *same* SCPI command interface. Each port has its
 * own RX ring buffer and its own in-progress command-line buffer, so a
 * partially-typed line on one port never interferes with the other.
 * A response to a command is always sent back out the same port the
 * command arrived on.
 */

typedef enum {
    UART_PORT_CONSOLE = 0,
    UART_PORT_USART1  = 1,
    UART_PORT_COUNT
} uart_port_t;

/**
 * Initialize both UART ports (devices, ring buffers, RX interrupts).
 */
void UART_Init(void);

/**
 * Send a single character via the given UART port (blocking poll-out).
 *
 * @param port Which physical UART to send on
 * @param c    Character to send
 */
void UART_SendChar(uart_port_t port, char c);

/**
 * Send a null-terminated string via the given UART port.
 *
 * @param port Which physical UART to send on
 * @param str  Pointer to string
 */
void UART_Send(uart_port_t port, const char *str);

/**
 * Send formatted output (like printf) via the given UART port.
 *
 * @param port   Which physical UART to send on
 * @param format Printf-style format string
 * @param ...    Arguments
 */
void UART_Printf(uart_port_t port, const char *format, ...);

/**
 * Attempt to read one character from the given UART port (non-blocking).
 *
 * Pulls from that port's RX ring buffer, which is filled by its own
 * RX interrupt handler.
 *
 * @param port Which physical UART to read from
 * @param c    Pointer to store received character
 * @return true if a character was received, false if none available
 */
bool UART_GetChar(uart_port_t port, uint8_t *c);

/**
 * Get the underlying Zephyr device pointer for a port (for direct
 * Zephyr API access if needed).
 */
const struct device *UART_GetDevice(uart_port_t port);

#endif // UART_H