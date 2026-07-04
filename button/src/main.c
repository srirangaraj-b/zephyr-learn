#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODE DT_ALIAS(led0)
#define BLUE_LED DT_ALIAS(led1)
#define RED_LED DT_ALIAS(led2)
#define BUTTON_NODE DT_ALIAS(sw0)

const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED, gpios);
const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED, gpios);

const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);



int main(){
    gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_HIGH);

    gpio_pin_configure_dt(&button, GPIO_INPUT);
    int x = 0;

    while(1){
        int button_value = gpio_pin_get_dt(&button);
        

        if (button_value == 1){
            if(x == 0){
                gpio_pin_set_dt(&green_led, 1);
                gpio_pin_set_dt(&blue_led, 1);
                gpio_pin_set_dt(&red_led, 1);
                x = 1;
            }else{
                gpio_pin_set_dt(&green_led, 0);
                gpio_pin_set_dt(&blue_led, 0);
                gpio_pin_set_dt(&red_led, 0);
                x = 0;
            }
        }

            
                 
            k_msleep(500);


    }
    return 0;
}