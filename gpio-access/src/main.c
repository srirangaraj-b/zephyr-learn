#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODEA DT_ALIAS(leda)
#define LED_NODEB DT_ALIAS(ledb)
#define LED_NODEC DT_ALIAS(ledc)

const struct gpio_dt_spec leda = GPIO_DT_SPEC_GET(LED_NODEA, gpios);
const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LED_NODEB, gpios);
const struct gpio_dt_spec ledc = GPIO_DT_SPEC_GET(LED_NODEC, gpios);


int main(void)
{
    gpio_pin_configure_dt(&leda, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ledc, GPIO_OUTPUT_HIGH);


    while (1) {
        gpio_pin_toggle_dt(&leda);
        gpio_pin_toggle_dt(&ledb);
        gpio_pin_toggle_dt(&ledc);

        k_msleep(500);
    }

    return 0;
}