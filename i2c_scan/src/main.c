/*
 * Bit-banged I2C (PE12=SCL, PH9=SDA) driving an AD5694 quad DAC,
 * stepped through a fixed voltage table on nucleo_n657x0_q / Zephyr RTOS.
 *
 * Voltage stepping is triggered from the serial console instead of a
 * physical button: send any character over the console UART (e.g. type
 * a key and hit Enter in your terminal, or just press any key if your
 * terminal is in raw mode) to advance to the next voltage.
 *
 * Pins are pulled from the "zephyr,user" node defined in the board overlay:
 *   gpios[0] = SCL = PE12
 *   gpios[1] = SDA = PH9
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

/* ============================= *
 *      I2C BIT-BANG CONFIG      *
 * ============================= */
#define I2C_DELAY_US 5U   /* ~100kHz; raise for longer wires / noisy bus */

static const struct gpio_dt_spec scl =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 0);
static const struct gpio_dt_spec sda =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 1);

/* Console UART used in place of the physical button */
static const struct device *const console_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* ============================= *
 *         AD5694 CONFIG         *
 * ============================= */
#define AD5694_I2C_ADDR         0x0C
#define AD5694_CMD_WRITE_UPDATE 0x3F

#define NUM_VOLTAGES 5
static const uint16_t dac_codes[NUM_VOLTAGES] = {
	0x000,   /* 0.00 V */
	0x441,   /* 1.30 V */
	0x854,   /* 2.50 V */
	0xC72,   /* 3.75 V */
	0xFFF    /* 5.00 V */
};
static const char *voltage_labels[NUM_VOLTAGES] = {
	"0.00 V",
	"1.30 V",
	"2.50 V",
	"3.75 V",
	"5.00 V"
};

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

/* ---------------- I2C / GPIO init ---------------- */
static int i2c_bitbang_init(void)
{
	if (!gpio_is_ready_dt(&scl) || !gpio_is_ready_dt(&sda)) {
		printk("GPIO controller not ready\n");
		return -ENODEV;
	}

	int ret;

	ret = gpio_pin_configure_dt(&scl, GPIO_OUTPUT_HIGH);
	if (ret) {
		printk("Failed to configure SCL (PE12): %d\n", ret);
		return ret;
	}

	/* GPIO_INPUT is OR'd in so the open-drain SDA line can be read back
	 * for ACK/NACK detection; dt_flags (OPEN_DRAIN | PULL_UP) from the
	 * overlay are merged in automatically by *_configure_dt().
	 */
	ret = gpio_pin_configure_dt(&sda, GPIO_OUTPUT_HIGH | GPIO_INPUT);
	if (ret) {
		printk("Failed to configure SDA (PH9): %d\n", ret);
		return ret;
	}

	return 0;
}

/* ---------------- I2C bus scan (diagnostic) ---------------- */
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

/* ---------------- AD5694 write ---------------- */
/*
 * Frame: [addr+W] [cmd] [D11..D4] [D3..D0 0000]
 * Returns true if every byte was ACKed.
 */
static bool AD5694_SetVoltage(uint16_t code)
{
	code &= 0x0FFF;  /* clamp to 12 bits */

	uint8_t msb = (uint8_t)((code >> 4) & 0xFF);   /* D[11:4]        */
	uint8_t lsb = (uint8_t)((code & 0x0F) << 4);   /* D[3:0] shifted */

	i2c_start();
	int ack_addr = i2c_write_byte((uint8_t)(AD5694_I2C_ADDR << 1)); /* +write bit */
	int ack_cmd  = i2c_write_byte(AD5694_CMD_WRITE_UPDATE);
	int ack_msb  = i2c_write_byte(msb);
	int ack_lsb  = i2c_write_byte(lsb);
	i2c_stop();

	return (ack_addr == 0 && ack_cmd == 0 && ack_msb == 0 && ack_lsb == 0);
}

/* ---------------- Serial console helper ---------------- */
/* Non-blocking: returns true if a byte was received (value discarded). */
static bool console_key_pressed(void)
{
	unsigned char c;

	return uart_poll_in(console_dev, &c) == 0;
}

/* ---------------- Main ---------------- */
int main(void)
{
	int ret = i2c_bitbang_init();

	if (ret) {
		return ret;
	}

	if (!device_is_ready(console_dev)) {
		printk("Console UART not ready\n");
		return -ENODEV;
	}

	printk("=============================\n");
	printk("  AD5694 DAC Voltage Stepper \n");
	printk("  Send any character over    \n");
	printk("  the serial console to step \n");
	printk("=============================\n");

	/* Optional: run a bus scan at boot to confirm the DAC is visible. */
	i2c_bitbang_scan();

	uint8_t step = 0;

	if (AD5694_SetVoltage(dac_codes[0])) {
		printk("[INIT] DAC set to %s\n", voltage_labels[0]);
	} else {
		printk("[ERROR] I2C communication failed!\n");
	}

	while (1) {
		if (console_key_pressed()) {
			step = (step + 1) % NUM_VOLTAGES;
			if (AD5694_SetVoltage(dac_codes[step])) {
				printk("[DAC] Output = %s  (code=0x%03X)\n",
				       voltage_labels[step], dac_codes[step]);
			} else {
				printk("[ERROR] I2C write failed!\n");
			}
		}
		k_sleep(K_MSEC(10));
	}

	return 0;
}