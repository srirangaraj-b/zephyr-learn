#ifndef SCPI_COMMANDS_H
#define SCPI_COMMANDS_H

#include "scpi_parser.h"
#include "application_daq.h"

/**
 * SCPI Command Handler Module
 *
 * Routes parsed SCPI commands to appropriate handlers.
 * Each command:
 *  1. Validates parameters
 *  2. Calls appropriate DAQ/driver functions
 *  3. Formats response
 *  4. Sends response via UART
 *
 * Error handling:
 *  - Invalid parameters → Add error to error queue
 *  - Range errors → Add error to error queue
 *  - Successful execution → Send response
 */

/**
 * Process a parsed SCPI command
 *
 * This is the main dispatch function.
 * Given a parsed command structure and DAQ state, executes the command
 * and generates a response string.
 *
 * The response string is stored in response buffer for transmission.
 *
 * @param cmd Parsed SCPI command
 * @param daq DAQ application state
 * @param response Output buffer for response string
 * @param response_size Size of response buffer
 * @return 0 on successful execution, -1 on error (error added to queue)
 */
int SCPI_ExecuteCommand(const scpi_command_t *cmd, daq_state_t *daq,
                        char *response, int response_size);

/**
 * Convenience function to generate standard OK response
 */
void SCPI_ResponseOK(char *buf, int size);

/**
 * Convenience function to generate numeric response
 */
void SCPI_ResponseFloat(char *buf, int size, float value, int decimals);

/**
 * Convenience function to generate string response
 */
void SCPI_ResponseString(char *buf, int size, const char *str);

#endif // SCPI_COMMANDS_H
