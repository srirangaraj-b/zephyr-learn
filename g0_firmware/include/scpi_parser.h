    #ifndef SCPI_PARSER_H
    #define SCPI_PARSER_H

    #include <stdint.h>
    #include <stdbool.h>

    /**
    * SCPI Command Parser Module
    *
    * Parses SCPI commands and queries from a command line string.
    *
    * SCPI command structure:
    *
    *  COMMAND:SUBCOMMAND:SUBSUBCOMMAND [parameters] [query?]
    *
    * Examples:
    *  "CONF:CURR (@1)"          → Configure ch1 as current
    *  "MEAS:CURR? (@1)"         → Query current on ch1
    *  "PID:KP 2.8"              → Set PID Kp to 2.8
    *  "SOUR:CURR 12"            → Set output current to 12mA
    *  "*IDN?"                   → Query identification
    *
    * This parser:
    *  - Is case-insensitive (normalizes input)
    *  - Extracts command hierarchy
    *  - Identifies if it's a query (ends with ?)
    *  - Extracts parameters (numeric, channel, etc.)
    *  - Detects syntax errors
    */

    /* Maximum sizes */
    #define SCPI_MAX_TOKENS 5
    #define SCPI_MAX_TOKEN_LEN 32
    #define SCPI_MAX_PARAMS 4

    /* SCPI command parsing results */
    typedef struct {
        char tokens[SCPI_MAX_TOKENS][SCPI_MAX_TOKEN_LEN];
        int token_count;
        bool is_query;      /* True if command ends with ? */
        char raw_params[128];  /* Raw parameter string */
        int param_count;
        float params[SCPI_MAX_PARAMS];  /* Parsed float parameters */
        uint8_t channel;    /* Parsed channel number (from @N) */
        bool has_channel;   /* Whether channel was specified */
    } scpi_command_t;

    /**
    * Parse a complete SCPI command line
    *
    * Parses the input string and extracts command tokens, query flag,
    * and parameters. Normalizes tokens to uppercase for comparison.
    *
    * Example inputs:
    *  "MEAS:CURR? (@1)"        → tokens=["MEAS","CURR"], is_query=true, channel=1
    *  "PID:KP 2.8"             → tokens=["PID","KP"], params[0]=2.8
    *  "CONF:VOLT (@2)"         → tokens=["CONF","VOLT"], channel=2
    *
    * @param line Input command line (null-terminated string)
    * @param cmd Output command structure to fill
    * @return 0 on success, negative on syntax error
    */
    int SCPI_Parse(const char *line, scpi_command_t *cmd);

    /**
    * Check if a parsed command matches a command tree
    *
    * Examples:
    *  SCPI_Match(cmd, "MEAS", "CURR")  → true if cmd is MEAS:CURR
    *  SCPI_Match(cmd, "PID", "KP")     → true if cmd is PID:KP
    *
    * @param cmd Parsed command structure
    * @param ... Variable number of command token strings, terminated by NULL
    * @return true if command matches, false otherwise
    */
    bool SCPI_Match(const scpi_command_t *cmd, const char *first, ...);

    /**
    * Get first parameter as float
    *
    * @param cmd Parsed command
    * @param index Parameter index (0 for first)
    * @param value Output pointer for value
    * @return 0 on success, negative on error (no parameter, parse error)
    */
    int SCPI_GetFloat(const scpi_command_t *cmd, int index, float *value);

    /**
    * Get first parameter as integer
    */
    int SCPI_GetInt(const scpi_command_t *cmd, int index, int *value);

    /**
    * Extract channel number from parameter (e.g., @1 from "(@1)")
    *
    * @param cmd Parsed command
    * @return Channel number (0-5), or -1 if not found/invalid
    */
    int SCPI_GetChannel(const scpi_command_t *cmd);

    /**
    * Debug: Print parsed command structure
    */
    void SCPI_DebugPrint(const scpi_command_t *cmd);

    #endif // SCPI_PARSER_H
