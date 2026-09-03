#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define USART3_NODE DT_NODELABEL(usart3)

static const struct device *usart3_dev = DEVICE_DT_GET(USART3_NODE);

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_console))
static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#else
static const struct device *console_dev = NULL;
#endif

int main(void)
{
    uint8_t rx_byte;
    uint8_t console_byte;

    if (!device_is_ready(usart3_dev)) {
        printk("USART3 is not ready\n");
        return 0;
    }

    printk("Zephyr USART3 Fast Passthrough Ready\n");

    while (1) {
        // Drain ALL pending bytes from USART3 RX and send to Console TX
        while (uart_poll_in(usart3_dev, &rx_byte) == 0) {
            
            if (console_dev && device_is_ready(console_dev)) {
                uart_poll_out(console_dev, rx_byte);
            }
        }

        // Drain ALL pending bytes from Console RX and send out to USART3 TX
        if (console_dev && device_is_ready(console_dev)) {
            while (uart_poll_in(console_dev, &console_byte) == 0) {
                uart_poll_out(usart3_dev, console_byte);
            }
        }

        // Yield CPU briefly to prevent lockup, but stay responsive
        k_yield();
    }
}