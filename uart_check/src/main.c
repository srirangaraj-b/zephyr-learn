#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

/*
 * On nucleo_n657x0_q, USART1 is the Zephyr console UART (PE5/PE6).
 * Don't attach a second ISR/consumer to it - use USART3 (PD8/PD9)
 * for your own link instead, enabled via the board overlay.
 */
#define COMMS_UART_NODE DT_NODELABEL(usart3)
static const struct device *comms_dev = DEVICE_DT_GET(COMMS_UART_NODE);

#define RX_BUF_SIZE 128
static char rx_buf[RX_BUF_SIZE];
static int rx_idx;

static void comms_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uart_irq_update(dev);

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

static void comms_send_line(const char *str)
{
	while (*str) {
		uart_poll_out(comms_dev, *str++);
	}
	uart_poll_out(comms_dev, '\n');
}

int main(void)
{
	if (!device_is_ready(comms_dev)) {
		printk("Comms UART (USART3) not ready\n");
		return -1;
	}

	uart_irq_callback_set(comms_dev, comms_uart_isr);
	uart_irq_rx_enable(comms_dev);

	while (1) {
		comms_send_line("Hello World from STM32N6");
		printk("Sent: Hello World from STM32N6\n");
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
