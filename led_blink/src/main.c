#include<zephyr/kernel.h>
#include<zephyr/drivers/gpio.h> //for using GPIO
#define THREAD_SIZE 1024

#define LED_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
void led_blink();
void button(); 

K_THREAD_DEFINE(LED, THREAD_SIZE, led_blink,NULL, NULL, NULL, 0, 0, 0 ); //(THREAD_NAME, THREAD_SIZE, FUNCTION, ARG1,2,3, PRIORITY, OPTIONS, DELAY)
K_THREAD_DEFINE(BUTTON, THREAD_SIZE, button,NULL, NULL, NULL, 5, 0, 0 );


//LED Blink Thread
void led_blink(){
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    while(1){
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }
}


// Button Input Thread
void button(){

    while(1){
        printk("button Simulation");
        k_msleep(1000);
    }
}

// Main Thread
int main(){
    printk("Welcome to Programming with Shriiii!!!\n");
    printk("Main Thread Entry Point \n");
    while(1){

        k_msleep(1000);
    }

    return 0;
}