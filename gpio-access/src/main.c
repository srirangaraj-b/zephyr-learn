#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODEA DT_ALIAS(leda)
#define LED_NODEB DT_ALIAS(ledb)

const struct gpio_dt_spec leda = GPIO_DT_SPEC_GET(LED_NODEA, gpios);
const struct gpio_dt_spec ledb = GPIO_DT_SPEC_GET(LED_NODEB, gpios);

const struct device *gpiog = DEVICE_DT_GET(DT_NODELABEL(gpiog)); //getting the GPIOG from the device tree

int main(void)
{
    gpio_pin_configure_dt(&leda, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ledb, GPIO_OUTPUT_HIGH);

    gpio_pin_configure(gpiog, 10, GPIO_OUTPUT_ACTIVE); //since we dont use the device tree we dont have to call the function

    while (1) {
        gpio_pin_toggle_dt(&leda);
        gpio_pin_toggle_dt(&ledb);

        gpio_pin_toggle(gpiog, 10); //Directly call the toggling from the GPIO Pin accessing
        //Since we call it using the direct pin accessing we cant use it for differnt boards which dont have the pin PG10

        k_msleep(500);
    }

    return 0;
}