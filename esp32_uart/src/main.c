#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

/* ESP32 UART2 - default pins TX2=GPIO17, RX2=GPIO16 */
#define UART2_NODE DT_NODELABEL(uart2)
static const struct device *uart2_dev = DEVICE_DT_GET(UART2_NODE);

#define RX_BUF_SIZE 128
static char rx_buf[RX_BUF_SIZE];
static int rx_idx;

static void uart2_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    /* Update the driver's internal interrupt status */
    uart_irq_update(dev);

    /* Check if data is ready to be read */
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

static void uart2_send_line(const char *str)
{
    while (*str) {
        uart_poll_out(uart2_dev, *str++);
    }
    uart_poll_out(uart2_dev, '\n');
}

int main(void)
{
    if (!device_is_ready(uart2_dev)) {
        printk("UART2 device not ready\n");
        return -1;
    }

    uart_irq_callback_set(uart2_dev, uart2_isr);
    uart_irq_rx_enable(uart2_dev);

    while (1) {
        uart2_send_line("Hello World from ESP32");
        printk("Sent: Hello World from ESP32\n");
        k_sleep(K_MSEC(1000));
    }

    return 0;
}