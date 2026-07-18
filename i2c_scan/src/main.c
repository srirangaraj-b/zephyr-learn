/*
 * Bit-banged I2C on PE12 (SCL) / PH9 (SDA) for nucleo_n657x0_q, Zephyr RTOS.
 *
 * Pins are pulled from the "zephyr,user" node defined in the board overlay:
 *   gpios[0] = SCL = PE12
 *   gpios[1] = SDA = PH9
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define I2C_DELAY_US 5U   /* ~100kHz; raise for longer wires / noisy bus */

static const struct gpio_dt_spec scl =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 0);
static const struct gpio_dt_spec sda =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 1);

/* ---------------- Pin helpers ---------------- */

static inline void scl_high(void) { gpio_pin_set_dt(&scl, 1); }
static inline void scl_low(void)  { gpio_pin_set_dt(&scl, 0); }
static inline void sda_high(void) { gpio_pin_set_dt(&sda, 1); }
static inline void sda_low(void)  { gpio_pin_set_dt(&sda, 0); }
static inline int  sda_read(void) { return gpio_pin_get_dt(&sda); }

/* ---------------- Bit-level primitives ---------------- */

static void i2c_start(void)
{
	sda_high();
	scl_high();
	k_busy_wait(I2C_DELAY_US);
	sda_low();
	k_busy_wait(I2C_DELAY_US);
	scl_low();
	k_busy_wait(I2C_DELAY_US);
}

static void i2c_stop(void)
{
	sda_low();
	k_busy_wait(I2C_DELAY_US);
	scl_high();
	k_busy_wait(I2C_DELAY_US);
	sda_high();
	k_busy_wait(I2C_DELAY_US);
}

/* Writes a byte MSB-first, returns 0 for ACK, 1 for NACK */
static int i2c_write_byte(uint8_t byte)
{
	for (int i = 0; i < 8; i++) {
		if (byte & 0x80) {
			sda_high();
		} else {
			sda_low();
		}
		k_busy_wait(I2C_DELAY_US);
		scl_high();
		k_busy_wait(I2C_DELAY_US);
		scl_low();
		byte <<= 1;
		k_busy_wait(I2C_DELAY_US);
	}

	/* release SDA so the slave can pull it low for ACK */
	sda_high();
	k_busy_wait(I2C_DELAY_US);
	scl_high();
	k_busy_wait(I2C_DELAY_US);
	int ack = (sda_read() == 0) ? 0 : 1;
	scl_low();
	k_busy_wait(I2C_DELAY_US);

	return ack;
}

/* ---------------- Init ---------------- */

static int i2c_bitbang_init(void)
{
	if (!gpio_is_ready_dt(&scl) || !gpio_is_ready_dt(&sda)) {
		printk("GPIO controller not ready\n");
		return -ENODEV;
	}

	int ret;

	/* GPIO_INPUT is OR'd in so the open-drain SDA line can be read back
	 * for ACK/NACK detection; dt_flags (OPEN_DRAIN | PULL_UP) from the
	 * overlay are merged in automatically by *_configure_dt().
	 */
	ret = gpio_pin_configure_dt(&scl, GPIO_OUTPUT_HIGH);
	if (ret) {
		printk("Failed to configure SCL (PE12): %d\n", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&sda, GPIO_OUTPUT_HIGH | GPIO_INPUT);
	if (ret) {
		printk("Failed to configure SDA (PH9): %d\n", ret);
		return ret;
	}

	return 0;
}

/* ---------------- Scan routine ---------------- */

static void i2c_bitbang_scan(void)
{
	uint8_t found = 0;

	printk("Scanning I2C bus (bit-banged: SCL=PE12, SDA=PH9)...\n");

	for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
		i2c_start();
		int ack = i2c_write_byte((uint8_t)(addr << 1)); /* addr + write bit */
		i2c_stop();

		if (ack == 0) {
			printk("  Device found at address: 0x%02X\n", addr);
			found++;
		}
	}

	if (found == 0) {
		printk("No I2C devices found.\n");
	} else {
		printk("Scan complete: %u device(s) found.\n", found);
	}
}

int main(void)
{
	int ret = i2c_bitbang_init();

	if (ret) {
		return ret;
	}

	i2c_bitbang_scan();

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}