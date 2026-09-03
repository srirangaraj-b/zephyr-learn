/*
 * ADC (6ch) + Bit-banged I2C AD5694 quad DAC, PID-smoothed, with
 * live gain tuning + character echo over the serial console.
 *
 * CH0 -> DAC A & DAC C (via pid_AC)
 * CH5 -> DAC B & DAC D (via pid_BD)
 *
 * Type a command and hit Enter, characters echo as you type:
 *   kp,ki,kd        -> sets Kp/Ki/Kd on BOTH loops
 *   AC kp ki kd      -> sets Kp/Ki/Kd on the CH0->A/C loop only
 *   BD kp ki kd      -> sets Kp/Ki/Kd on the CH5->B/D loop only
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <string.h>

/* ============================= *
 *      I2C BIT-BANG CONFIG      *
 * ============================= */
#define I2C_DELAY_US 5U

static const struct gpio_dt_spec scl =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 0);
static const struct gpio_dt_spec sda =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), gpios, 1);

/* ============================= *
 *         AD5694 CONFIG         *
 * ============================= */
#define AD5694_I2C_ADDR   0x0C

#define AD5694_CMD_WRITE_UPDATE_A 0x31
#define AD5694_CMD_WRITE_UPDATE_B 0x32
#define AD5694_CMD_WRITE_UPDATE_C 0x34
#define AD5694_CMD_WRITE_UPDATE_D 0x38

#define DAC_VREF 5.0f

static inline void scl_high(void) { gpio_pin_set_dt(&scl, 1); }
static inline void scl_low(void)  { gpio_pin_set_dt(&scl, 0); }
static inline void sda_high(void) { gpio_pin_set_dt(&sda, 1); }
static inline void sda_low(void)  { gpio_pin_set_dt(&sda, 0); }
static inline int  sda_read(void) { return gpio_pin_get_dt(&sda); }

static void i2c_start(void)
{
	sda_high(); scl_high(); k_busy_wait(I2C_DELAY_US);
	sda_low();  k_busy_wait(I2C_DELAY_US);
	scl_low();  k_busy_wait(I2C_DELAY_US);
}

static void i2c_stop(void)
{
	sda_low();  k_busy_wait(I2C_DELAY_US);
	scl_high(); k_busy_wait(I2C_DELAY_US);
	sda_high(); k_busy_wait(I2C_DELAY_US);
}

static int i2c_write_byte(uint8_t byte)
{
	for (int i = 0; i < 8; i++) {
		if (byte & 0x80) sda_high(); else sda_low();
		k_busy_wait(I2C_DELAY_US);
		scl_high(); k_busy_wait(I2C_DELAY_US);
		scl_low();
		byte <<= 1;
		k_busy_wait(I2C_DELAY_US);
	}
	sda_high(); k_busy_wait(I2C_DELAY_US);
	scl_high(); k_busy_wait(I2C_DELAY_US);
	int ack = (sda_read() == 0) ? 0 : 1;
	scl_low(); k_busy_wait(I2C_DELAY_US);
	return ack;
}

static int i2c_bitbang_init(void)
{
	if (!gpio_is_ready_dt(&scl) || !gpio_is_ready_dt(&sda)) {
		printk("GPIO controller not ready\n");
		return -ENODEV;
	}
	int ret = gpio_pin_configure_dt(&scl, GPIO_OUTPUT_HIGH);
	if (ret) return ret;
	ret = gpio_pin_configure_dt(&sda, GPIO_OUTPUT_HIGH | GPIO_INPUT);
	if (ret) return ret;
	return 0;
}

static bool AD5694_WriteChannel(uint8_t channel_cmd, uint16_t code)
{
	code &= 0x0FFF;
	uint8_t msb = (uint8_t)((code >> 4) & 0xFF);
	uint8_t lsb = (uint8_t)((code & 0x0F) << 4);

	i2c_start();
	int ack_addr = i2c_write_byte((uint8_t)(AD5694_I2C_ADDR << 1));
	int ack_cmd  = i2c_write_byte(channel_cmd);
	int ack_msb  = i2c_write_byte(msb);
	int ack_lsb  = i2c_write_byte(lsb);
	i2c_stop();

	return (ack_addr == 0 && ack_cmd == 0 && ack_msb == 0 && ack_lsb == 0);
}

static inline uint16_t voltage_to_code(float v)
{
	if (v < 0.0f) v = 0.0f;
	if (v > DAC_VREF) v = DAC_VREF;
	return (uint16_t)((v / DAC_VREF) * 4095.0f + 0.5f);
}

/* ============================= *
 *          ADC CONFIG           *
 * ============================= */
#define NUM_CHANNELS 6

static const struct adc_dt_spec adc_ch[NUM_CHANNELS] = {
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 4),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 5),
};

static int32_t calibrated_mv(uint16_t raw)
{
	return ((int32_t)raw * 5000) / 2935;
}

/* ============================= *
 *          PID CONFIG           *
 * ============================= */
typedef struct {
	float kp, ki, kd;
	float integral;
	float prev_error;
	float output;
} pid_t;

#define PID_PERIOD_MS 100
#define PID_DT        (PID_PERIOD_MS / 1000.0f)
#define INTEGRAL_MAX  10.0f

static pid_t pid_AC = { .kp = 0.8f, .ki = 0.5f, .kd = 0.05f };
static pid_t pid_BD = { .kp = 0.8f, .ki = 0.5f, .kd = 0.05f };

static float pid_update(pid_t *p, float setpoint, float dt)
{
	float error = setpoint - p->output;

	p->integral += error * dt;
	if (p->integral > INTEGRAL_MAX) p->integral = INTEGRAL_MAX;
	if (p->integral < -INTEGRAL_MAX) p->integral = -INTEGRAL_MAX;

	float derivative = (dt > 0.0f) ? (error - p->prev_error) / dt : 0.0f;
	p->prev_error = error;

	float out = p->output + (p->kp * error) + (p->ki * p->integral) + (p->kd * derivative);
	if (out < 0.0f) out = 0.0f;
	if (out > DAC_VREF) out = DAC_VREF;

	p->output = out;
	return out;
}

static void apply_gains(pid_t *p, float kp, float ki, float kd)
{
	p->kp = kp;
	p->ki = ki;
	p->kd = kd;
	p->integral = 0.0f;
	p->prev_error = 0.0f;
}

/* ============================= *
 *      SERIAL COMMAND INPUT     *
 * ============================= */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define BUFFER_SIZE 128

static const struct device *uart_dev;
static char line_buf[BUFFER_SIZE];
static int line_pos = 0;

static bool parse_three_floats(const char *str, float *a, float *b, float *c)
{
	char *end;

	*a = strtof(str, &end);
	if (end == str) return false;
	str = end;
	while (*str == ',' || *str == ' ') str++;

	*b = strtof(str, &end);
	if (end == str) return false;
	str = end;
	while (*str == ',' || *str == ' ') str++;

	*c = strtof(str, &end);
	if (end == str) return false;

	return true;
}

static void process_command(char *line)
{
	char *s = line;
	while (*s == ' ') s++;
	if (*s == '\0') return;

	float kp, ki, kd;

	if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'C' || s[1] == 'c')) {
		if (parse_three_floats(s + 2, &kp, &ki, &kd)) {
			apply_gains(&pid_AC, kp, ki, kd);
			printk("[PID] AC loop -> Kp=%.3f Ki=%.3f Kd=%.3f\n", kp, ki, kd);
		} else {
			printk("[PID] Bad format. Use: AC kp ki kd\n");
		}
		return;
	}

	if ((s[0] == 'B' || s[0] == 'b') && (s[1] == 'D' || s[1] == 'd')) {
		if (parse_three_floats(s + 2, &kp, &ki, &kd)) {
			apply_gains(&pid_BD, kp, ki, kd);
			printk("[PID] BD loop -> Kp=%.3f Ki=%.3f Kd=%.3f\n", kp, ki, kd);
		} else {
			printk("[PID] Bad format. Use: BD kp ki kd\n");
		}
		return;
	}

	if (parse_three_floats(s, &kp, &ki, &kd)) {
		apply_gains(&pid_AC, kp, ki, kd);
		apply_gains(&pid_BD, kp, ki, kd);
		printk("[PID] Both loops -> Kp=%.3f Ki=%.3f Kd=%.3f\n", kp, ki, kd);
		return;
	}

	printk("[PID] Unrecognized: \"%s\"\n", s);
	printk("      Usage: 'kp,ki,kd'  |  'AC kp ki kd'  |  'BD kp ki kd'\n");
}

/* Called every 1ms from main. Echoes as you type, dispatches on Enter. */
static void console_poll_and_handle(void)
{
	unsigned char c;

	if (uart_poll_in(uart_dev, &c) == 0) {

		if (c == '\r' || c == '\n') {
			line_buf[line_pos] = '\0';
			uart_poll_out(uart_dev, '\n');
			if (line_pos > 0) {
				process_command(line_buf);
			}
			line_pos = 0;
			uart_poll_out(uart_dev, '>');
			uart_poll_out(uart_dev, ' ');
		}
		else if (c == '\b' || c == 127) {
			if (line_pos > 0) {
				line_pos--;
				uart_poll_out(uart_dev, '\b');
				uart_poll_out(uart_dev, ' ');
				uart_poll_out(uart_dev, '\b');
			}
		}
		else {
			if (line_pos < BUFFER_SIZE - 1) {
				line_buf[line_pos++] = (char)c;
				uart_poll_out(uart_dev, c);   /* echo */
			}
		}
	}
}

/* ---------------- Main ---------------- */
int main(void)
{
	int ret = i2c_bitbang_init();
	if (ret) {
		printk("I2C bit-bang init failed: %d\n", ret);
		return ret;
	}

	uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
	if (!device_is_ready(uart_dev)) {
		printk("UART not ready!\n");
		return 0;
	}

	for (int i = 0; i < NUM_CHANNELS; i++) {
		ret = adc_channel_setup_dt(&adc_ch[i]);
		if (ret < 0) {
			printk("ADC channel %d setup failed: %d\n", i, ret);
			return ret;
		}
	}

	printk("\nReady. Enter PID gains: 'kp,ki,kd' | 'AC kp ki kd' | 'BD kp ki kd'\n> ");

	float adc_v[NUM_CHANNELS];
	uint32_t buf;
	struct adc_sequence seq = { .buffer = &buf, .buffer_size = sizeof(buf) };

	int64_t next_pid_run = k_uptime_get() + PID_PERIOD_MS;

	while (1) {
		/* Poll UART every ~1ms so typing feels instant, never blocks */
		console_poll_and_handle();

		/* Run the ADC->PID->DAC cycle only every PID_PERIOD_MS,
		 * without blocking the UART poll above. */
		if (k_uptime_get() >= next_pid_run) {
			next_pid_run += PID_PERIOD_MS;

			for (int i = 0; i < NUM_CHANNELS; i++) {
				adc_sequence_init_dt(&adc_ch[i], &seq);
				ret = adc_read_dt(&adc_ch[i], &seq);
				if (ret < 0) {
					adc_v[i] = 0.0f;
					continue;
				}
				uint16_t raw = (uint16_t)(buf & 0xFFFF);
				adc_v[i] = calibrated_mv(raw) / 1000.0f;
			}

			float out_AC = pid_update(&pid_AC, adc_v[0], PID_DT);
			float out_BD = pid_update(&pid_BD, adc_v[5], PID_DT);

			uint16_t code_AC = voltage_to_code(out_AC);
			uint16_t code_BD = voltage_to_code(out_BD);

			AD5694_WriteChannel(AD5694_CMD_WRITE_UPDATE_A, code_AC);
			AD5694_WriteChannel(AD5694_CMD_WRITE_UPDATE_C, code_AC);
			AD5694_WriteChannel(AD5694_CMD_WRITE_UPDATE_B, code_BD);
			AD5694_WriteChannel(AD5694_CMD_WRITE_UPDATE_D, code_BD);
		}

		k_msleep(1);
	}

	return 0;
}