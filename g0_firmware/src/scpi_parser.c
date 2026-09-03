    #include "scpi_parser.h"
    #include <string.h>
    #include <ctype.h>
    #include <stdlib.h>
    #include <stdarg.h>
    #include <stdio.h>

    /**
    * SCPI Parser Implementation
    *
    * Tokenizes SCPI commands and extracts structured information.
    */

    static void str_uppercase(char *str)
    {
        if (!str) return;
        while (*str) {
            *str = toupper((unsigned char)*str);
            str++;
        }
    }

    static void skip_whitespace(const char **str)
    {
        while (*str && isspace((unsigned char)**str)) {
            (*str)++;
        }
    }

    int SCPI_Parse(const char *line, scpi_command_t *cmd)
    {
        if (!line || !cmd) {
            return -1;
        }
        
        memset(cmd, 0, sizeof(*cmd));
        
        const char *p = line;
        skip_whitespace(&p);
        
        if (*p == '\0') {
            return -1;  /* Empty line */
        }
        
        /* Parse tokens separated by : or space */
        cmd->token_count = 0;
        
        while (*p && cmd->token_count < SCPI_MAX_TOKENS) {
            skip_whitespace(&p);
            if (*p == '\0') break;
            
            /* Check for query marker (?) at end */
            if (*p == '?') {
                cmd->is_query = true;
                p++;
                skip_whitespace(&p);
                break;
            }
            
            /* Extract token (alphanumeric and some special chars) */
            int token_len = 0;
            char token[SCPI_MAX_TOKEN_LEN];
            
            while (token_len < SCPI_MAX_TOKEN_LEN - 1 && *p && 
                (isalnum((unsigned char)*p) || *p == '_' || *p == '*')) {
                token[token_len++] = *p++;
            }
            
            if (token_len == 0) {
                /* Try to skip unknown characters */
                if (*p) p++;
                continue;
            }
            
            token[token_len] = '\0';
            str_uppercase(token);
            
            strcpy(cmd->tokens[cmd->token_count], token);
            cmd->token_count++;
            
            /* Check for : separator or parameters */
            skip_whitespace(&p);
            if (*p == ':') {
                p++;
            } else if (*p == '?') {
                cmd->is_query = true;
                p++;
                break;
            } else {
                /* Parameters follow */
                break;
            }
        }
        
        if (cmd->token_count == 0) {
            return -1;
        }
        
        /* Extract parameters (everything after command tokens) */
        skip_whitespace(&p);
        if (*p) {
            /* Check for query marker after parameters */
            const char *param_end = p;
            while (*param_end && *param_end != '?') {
                param_end++;
            }
            
            int param_len = param_end - p;
            if (param_len > 0 && param_len < (int)sizeof(cmd->raw_params)) {
                strncpy(cmd->raw_params, p, param_len);
                cmd->raw_params[param_len] = '\0';
            }
            
            if (*param_end == '?') {
                cmd->is_query = true;
            }
        }
        
        /* Parse raw parameters for channel (@N) */
        p = cmd->raw_params;
        skip_whitespace(&p);
        
        if (*p == '(' && *(p + 1) == '@') {
            /* Channel designation: (@1) */
            int ch = atoi(p + 2);
            if (ch >= 0 && ch <= 255) {
                cmd->channel = ch;
                cmd->has_channel = true;
            }
        }
        
        /* Parse numeric parameters */
        cmd->param_count = 0;
        p = cmd->raw_params;
        
        while (*p && cmd->param_count < SCPI_MAX_PARAMS) {
            skip_whitespace(&p);
            if (*p == '\0') break;
            
            /* Skip non-numeric characters (e.g., parentheses, commas) */
            if (*p == '(' || *p == ')' || *p == '@' || *p == ',') {
                p++;
                continue;
            }
            
            /* Try to parse a number */
            char *end;
            float val = strtof(p, &end);
            
            if (end == p) {
                /* No number found */
                p++;
                continue;
            }
            
            cmd->params[cmd->param_count++] = val;
            p = end;
        }
        
        return 0;
    }

    bool SCPI_Match(const scpi_command_t *cmd, const char *first, ...)
    {
        if (!cmd || !first) {
            return false;
        }
        
        int expected_tokens = 0;
        va_list args;
        va_start(args, first);
        
        const char *token = first;
        while (token && expected_tokens < SCPI_MAX_TOKENS) {
            char normalized[SCPI_MAX_TOKEN_LEN];
            strcpy(normalized, token);
            str_uppercase(normalized);
            
            if (expected_tokens >= cmd->token_count) {
                va_end(args);
                return false;
            }
            
            if (strcmp(normalized, cmd->tokens[expected_tokens]) != 0) {
                va_end(args);
                return false;
            }
            
            expected_tokens++;
            token = va_arg(args, const char *);
        }
        
        va_end(args);
        
        /* Check that we matched exactly the right number of tokens */
        return expected_tokens == cmd->token_count;
    }

    int SCPI_GetFloat(const scpi_command_t *cmd, int index, float *value)
    {
        if (!cmd || !value) {
            return -1;
        }
        
        if (index < 0 || index >= cmd->param_count) {
            return -1;
        }
        
        *value = cmd->params[index];
        return 0;
    }

    int SCPI_GetInt(const scpi_command_t *cmd, int index, int *value)
    {
        if (!cmd || !value) {
            return -1;
        }
        
        if (index < 0 || index >= cmd->param_count) {
            return -1;
        }
        
        *value = (int)cmd->params[index];
        return 0;
    }

    int SCPI_GetChannel(const scpi_command_t *cmd)
    {
        if (!cmd || !cmd->has_channel) {
            return -1;
        }
        return cmd->channel;
    }

    void SCPI_DebugPrint(const scpi_command_t *cmd)
    {
        if (!cmd) return;
        
        printf("[SCPI] Tokens: ");
        for (int i = 0; i < cmd->token_count; i++) {
            printf("%s ", cmd->tokens[i]);
        }
        printf("\n");
        printf("[SCPI] Query: %s\n", cmd->is_query ? "YES" : "NO");
        printf("[SCPI] Params: ");
        for (int i = 0; i < cmd->param_count; i++) {
            printf("%f ", cmd->params[i]);
        }
        printf("\n");
        printf("[SCPI] Channel: %s (%d)\n", 
            cmd->has_channel ? "YES" : "NO", cmd->channel);
    }
