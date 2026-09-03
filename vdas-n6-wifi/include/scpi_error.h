#ifndef SCPI_ERROR_H
#define SCPI_ERROR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * SCPI Error Queue Module
 *
 * Maintains a queue of SCPI errors as required by the standard.
 * Each error has a numeric code and a message string.
 *
 * Error behavior:
 *  - SCPI:ERR? returns oldest error and removes it
 *  - Subsequent SCPI:ERR? returns next error
 *  - When queue is empty, returns 0,"No error"
 *  - Maximum of ~16 errors can be buffered
 *  - *CLS clears all errors
 *  - *RST clears all errors
 */

typedef struct {
    int code;
    char message[64];
} scpi_error_t;

/**
 * Initialize error queue
 */
void SCPI_Error_Init(void);

/**
 * Add an error to the queue
 *
 * @param code SCPI error code (e.g., -222 for out of range)
 * @param message Error description (will be truncated to 63 chars)
 */
void SCPI_Error_Add(int code, const char *message);

/**
 * Get oldest error and remove it from queue
 *
 * @param error Pointer to error structure to fill
 * @return true if an error was returned, false if queue is empty
 */
bool SCPI_Error_Get(scpi_error_t *error);

/**
 * Query if there are any errors in the queue
 *
 * @return true if queue is not empty, false otherwise
 */
bool SCPI_Error_HasError(void);

/**
 * Clear all errors from the queue
 * Called by *CLS or *RST
 */
void SCPI_Error_Clear(void);

/**
 * Convenience function: Format error response string
 *
 * Formats "code,\"message\"" suitable for UART transmission
 *
 * @param error Pointer to error structure
 * @param buf Output buffer
 * @param buf_size Size of output buffer
 */
void SCPI_Error_Format(const scpi_error_t *error, char *buf, int buf_size);

#endif // SCPI_ERROR_H
