#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>


#define OUTPUT_NODE1 DT_ALIAS(op1)
#define OUTPUT_NODE2 DT_ALIAS(op2)
#define OUTPUT_NODE3 DT_ALIAS(op3)
#define OUTPUT_NODE4 DT_ALIAS(op4)
#define OUTPUT_NODE5 DT_ALIAS(op5)
#define OUTPUT_NODE6 DT_ALIAS(op6)
#define OUTPUT_NODE7 DT_ALIAS(op7)
#define OUTPUT_NODE8 DT_ALIAS(op8)
#define OUTPUT_NODE9 DT_ALIAS(op9)
#define OUTPUT_NODE10 DT_ALIAS(op10)
#define OUTPUT_NODE11 DT_ALIAS(op11)


const struct gpio_dt_spec op1  = GPIO_DT_SPEC_GET(OUTPUT_NODE1 ,gpios);
const struct gpio_dt_spec op2  = GPIO_DT_SPEC_GET(OUTPUT_NODE2 ,gpios);
const struct gpio_dt_spec op3  = GPIO_DT_SPEC_GET(OUTPUT_NODE3 ,gpios);
const struct gpio_dt_spec op4  = GPIO_DT_SPEC_GET(OUTPUT_NODE4 ,gpios);
const struct gpio_dt_spec op5  = GPIO_DT_SPEC_GET(OUTPUT_NODE5 ,gpios);
const struct gpio_dt_spec op6  = GPIO_DT_SPEC_GET(OUTPUT_NODE6 ,gpios);
const struct gpio_dt_spec op7  = GPIO_DT_SPEC_GET(OUTPUT_NODE7 ,gpios);
const struct gpio_dt_spec op8  = GPIO_DT_SPEC_GET(OUTPUT_NODE8 ,gpios);
const struct gpio_dt_spec op9  = GPIO_DT_SPEC_GET(OUTPUT_NODE9 ,gpios);
const struct gpio_dt_spec op10  = GPIO_DT_SPEC_GET(OUTPUT_NODE10 ,gpios);
const struct gpio_dt_spec op11  = GPIO_DT_SPEC_GET(OUTPUT_NODE11 ,gpios);

int main(void)
{
	gpio_pin_configure_dt(&op1 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op2 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op3 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op4 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op5 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op6 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op7 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op8 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op9 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op10 ,GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&op11 ,GPIO_OUTPUT_HIGH);

	while (1) {
		gpio_pin_toggle_dt(&op1);
		gpio_pin_toggle_dt(&op2);
		gpio_pin_toggle_dt(&op3);
		gpio_pin_toggle_dt(&op4);
		gpio_pin_toggle_dt(&op5);
		gpio_pin_toggle_dt(&op6);
		gpio_pin_toggle_dt(&op7);
		gpio_pin_toggle_dt(&op8);
		gpio_pin_toggle_dt(&op9);
		gpio_pin_toggle_dt(&op10);
		gpio_pin_toggle_dt(&op11);

		k_msleep(1000);
	}

	return 0;
}