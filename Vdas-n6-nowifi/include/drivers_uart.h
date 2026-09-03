#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

/**
 * UART Driver Module
 *
 * This module provides simple UART I/O functions.
 * For the reference implementation, this is a mock that uses stdin/stdout.
 * For real hardware, replace UART_Send() and UART_GetChar() implementations.
 */

/**
 * Initialize UART (mock does nothing, real hardware would set up registers)
 */
void UART_Init(void);

/**
 * Send a single character via UART
 *
 * In real implementation, this writes to UART TX register.
 * For mock, this goes to stdout.
 *
 * @param c Character to send
 */
void UART_SendChar(char c);

/**
 * Send a null-terminated string via UART
 *
 * @param str Pointer to string
 */
void UART_Send(const char *str);

/**
 * Send formatted output (like printf)
 *
 * @param format Printf-style format string
 * @param ... Arguments
 */
void UART_Printf(const char *format, ...);

/**
 * Attempt to read one character from UART (non-blocking)
 *
 * In real implementation, this checks if data is available in RX register.
 * For mock, this checks stdin.
 *
 * @param c Pointer to store received character
 * @return true if character was received, false if no data available
 */
bool UART_GetChar(uint8_t *c);

#endif // UART_H
