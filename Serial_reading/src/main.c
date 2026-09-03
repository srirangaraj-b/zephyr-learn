#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define BUFFER_SIZE 128

int main(void)
{
    const struct device *uart = DEVICE_DT_GET(UART_DEVICE_NODE);

    if (!device_is_ready(uart)) {
        printk("UART not ready!\n");
        return 0;
    }

    char buffer[BUFFER_SIZE];
    int pos = 0;

    printk("\nUART input test\n");
    printk("Type something and press Enter:\n> ");

    while (1) {
        unsigned char c;

        if (uart_poll_in(uart, &c) == 0) {

            /* Enter key */
            if (c == '\r' || c == '\n') {

                buffer[pos] = '\0';

                printk("\nYou entered: %s\n", buffer);

                pos = 0;

                printk("> ");
            }

            /* Backspace */
            else if (c == '\b' || c == 127) {
                if (pos > 0) {
                    pos--;
                    uart_poll_out(uart, '\b');
                    uart_poll_out(uart, ' ');
                    uart_poll_out(uart, '\b');
                }
            }

            /* Normal character */
            else {
                if (pos < BUFFER_SIZE - 1) {
                    buffer[pos++] = c;

                    /* Echo character */
                    uart_poll_out(uart, c);
                }
            }
        }

        k_msleep(1);
    }

    return 0;
}