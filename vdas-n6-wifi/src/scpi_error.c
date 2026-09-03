#include "scpi_error.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define ERROR_QUEUE_SIZE 16

typedef struct {
    scpi_error_t queue[ERROR_QUEUE_SIZE];
    int head;  /* Next position to write */
    int count; /* Number of errors in queue */
} error_queue_t;

static error_queue_t error_queue = {0};

void SCPI_Error_Init(void)
{
    error_queue.head = 0;
    error_queue.count = 0;
}

void SCPI_Error_Add(int code, const char *message)
{
    if (!message) return;
    
    /* If queue is full, we need to drop the oldest error */
    if (error_queue.count >= ERROR_QUEUE_SIZE) {
        error_queue.count--;
    }
    
    /* Add new error at head position */
    int index = error_queue.head;
    error_queue.queue[index].code = code;
    
    strncpy(error_queue.queue[index].message, message, 
            sizeof(error_queue.queue[index].message) - 1);
    error_queue.queue[index].message[sizeof(error_queue.queue[index].message) - 1] = '\0';
    
    /* Move head to next position (circular) */
    error_queue.head = (error_queue.head + 1) % ERROR_QUEUE_SIZE;
    error_queue.count++;
}

bool SCPI_Error_Get(scpi_error_t *error)
{
    if (!error) return false;
    if (error_queue.count == 0) {
        /* Queue is empty, return "no error" */
        error->code = 0;
        strcpy(error->message, "No error");
        return false;
    }
    
    /* Calculate index of oldest error */
    int oldest_index = (error_queue.head - error_queue.count + ERROR_QUEUE_SIZE) % ERROR_QUEUE_SIZE;
    
    /* Copy oldest error */
    *error = error_queue.queue[oldest_index];
    
    /* Remove oldest error from queue */
    error_queue.count--;
    
    return true;
}

bool SCPI_Error_HasError(void)
{
    return error_queue.count > 0;
}

void SCPI_Error_Clear(void)
{
    error_queue.head = 0;
    error_queue.count = 0;
}

void SCPI_Error_Format(const scpi_error_t *error, char *buf, int buf_size)
{
    if (!error || !buf || buf_size < 10) {
        return;
    }
    
    snprintf(buf, buf_size, "%d,\"%s\"", error->code, error->message);
}
