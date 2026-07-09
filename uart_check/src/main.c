#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#define UART1_NODE DT_NODELABEL(usart1)
static const struct device *uart1_dev = DEVICE_DT_GET(UART1_NODE);

#define RX_BUF_SIZE 128
static char rx_buf[RX_BUF_SIZE];
static int rx_idx;

static void uart1_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    // Call this to update the driver's internal interrupt status
    uart_irq_update(dev);

    // Now check if data is ready to be read
    if (uart_irq_rx_ready(dev)) {
        uint8_t c;

        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                if (rx_idx > 0) {
                    rx_buf[rx_idx] = '\0';
                    printk("Received: %s\n", rx_buf);
                    rx_idx = 0;
                }
            } else if (rx_idx < RX_BUF_SIZE - 1) {
                rx_buf[rx_idx++] = c;
            } else {
                /* overflow guard: reset */
                rx_idx = 0;
            }
        }
    }
}

static void uart1_send_line(const char *str)
{
    while (*str) {
        uart_poll_out(uart1_dev, *str++);
    }
    uart_poll_out(uart1_dev, '\n');
}

int main(void)
{
    if (!device_is_ready(uart1_dev)) {
        printk("UART1 device not ready\n");
        return -1;
    }

    uart_irq_callback_set(uart1_dev, uart1_isr);
    uart_irq_rx_enable(uart1_dev);

    while (1) {
        uart1_send_line("Hello World from STM32");
        printk("Sent: Hello World from STM32\n");
        k_sleep(K_MSEC(1000));
    }

    return 0;
}