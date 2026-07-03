#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODE DT_ALIAS(led0)

const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

int main(){

    printk("Program with Shriiii!!!!");
    printk("LED Flashing Program");
    
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_HIGH);

    while(1){
        gpio_pin_toggle_dt(&led);
        k_msleep(500);

    }

    return 0;
}