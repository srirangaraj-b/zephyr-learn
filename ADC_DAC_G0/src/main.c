#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#define USER_NODE DT_PATH(zephyr_user)

/* ======================= ADC ======================= */
static const struct adc_dt_spec adc_channels[] = {
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0), /* CH1 - PA0  */
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 1), /* CH2 - PA1  */
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 2), /* CH3 - PA4  */
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 3), /* CH4 - PB1  */
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 4), /* CH5 - PB11 */
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 5), /* CH6 - PB12 */
};

static const char *channel_names[] = {
    "CH1(PA0)", "CH2(PA1)", "CH3(PA4)",
    "CH4(PB1)", "CH5(PB11)", "CH6(PB12)"
};

/* ======================= I2C bit-bang (SCL=PB8, SDA=PC9) ======================= */
#define I2C_DELAY_US 5U

static const struct gpio_dt_spec scl =
    GPIO_DT_SPEC_GET_BY_IDX(USER_NODE, gpios, 0); /* SCL = PB8 */
static const struct gpio_dt_spec sda =
    GPIO_DT_SPEC_GET_BY_IDX(USER_NODE, gpios, 1); /* SDA = PC9 */

static const struct device *const console_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

#define AD5694_I2C_ADDR         0x0C
#define AD5694_CMD_WRITE_UPDATE 0x3F

#define NUM_VOLTAGES 5
static const uint16_t dac_codes[NUM_VOLTAGES] = {
    0x000, 0x441, 0x854, 0xC72, 0xFFF
};
static const char *voltage_labels[NUM_VOLTAGES] = {
    "0.00 V", "1.30 V", "2.50 V", "3.75 V", "5.00 V"
};

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
        (byte & 0x80) ? sda_high() : sda_low();
        k_busy_wait(I2C_DELAY_US);
        scl_high(); k_busy_wait(I2C_DELAY_US);
        scl_low();  byte <<= 1;
        k_busy_wait(I2C_DELAY_US);
    }
    sda_high(); k_busy_wait(I2C_DELAY_US);
    scl_high(); k_busy_wait(I2C_DELAY_US);
    int ack = (sda_read() == 0) ? 0 : 1;
    scl_low();  k_busy_wait(I2C_DELAY_US);
    return ack;
}

static int i2c_bitbang_init(void)
{
    if (!gpio_is_ready_dt(&scl) || !gpio_is_ready_dt(&sda)) {
        printk("I2C GPIO controller not ready\n");
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&scl, GPIO_OUTPUT_HIGH);
    if (ret) { printk("Failed to configure SCL (PB8): %d\n", ret); return ret; }

    ret = gpio_pin_configure_dt(&sda, GPIO_OUTPUT_HIGH | GPIO_INPUT);
    if (ret) { printk("Failed to configure SDA (PC9): %d\n", ret); return ret; }

    return 0;
}

static bool AD5694_SetVoltage(uint16_t code)
{
    code &= 0x0FFF;
    uint8_t msb = (uint8_t)((code >> 4) & 0xFF);
    uint8_t lsb = (uint8_t)((code & 0x0F) << 4);

    i2c_start();
    int ack_addr = i2c_write_byte((uint8_t)(AD5694_I2C_ADDR << 1));
    int ack_cmd  = i2c_write_byte(AD5694_CMD_WRITE_UPDATE);
    int ack_msb  = i2c_write_byte(msb);
    int ack_lsb  = i2c_write_byte(lsb);
    i2c_stop();

    return (ack_addr == 0 && ack_cmd == 0 && ack_msb == 0 && ack_lsb == 0);
}

static bool console_key_pressed(void)
{
    unsigned char c;
    return uart_poll_in(console_dev, &c) == 0;
}

/* ======================= main ======================= */
int main(void)
{
    /* --- ADC init --- */
    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk("ERROR: ADC controller for %s not ready\n", channel_names[i]);
            return 0;
        }
        int err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk("ERROR: Could not setup %s (err %d)\n", channel_names[i], err);
            return 0;
        }
    }

    /* --- I2C / DAC init --- */
    if (i2c_bitbang_init() < 0) {
        return 0;
    }
    if (!device_is_ready(console_dev)) {
        printk("Console UART not ready\n");
        return 0;
    }

    printk("ADC + I2C-DAC combined app ready\n");
    printk("Send any char over console to step DAC voltage\n\n");

    uint8_t step = 0;
    if (AD5694_SetVoltage(dac_codes[0])) {
        printk("[INIT] DAC set to %s\n", voltage_labels[0]);
    } else {
        printk("[ERROR] Initial DAC write failed!\n");
    }

    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    int64_t last_adc_print = 0;

    while (1) {
        /* --- Step DAC on keypress --- */
        if (console_key_pressed()) {
            step = (step + 1) % NUM_VOLTAGES;
            if (AD5694_SetVoltage(dac_codes[step])) {
                printk("[DAC] Output = %s (code=0x%03X)\n",
                       voltage_labels[step], dac_codes[step]);
            } else {
                printk("[ERROR] DAC write failed!\n");
            }
        }

        /* --- Read ADC every 1s --- */
        if (k_uptime_get() - last_adc_print >= 1000) {
            for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
                (void)adc_sequence_init_dt(&adc_channels[i], &sequence);
                int err = adc_read_dt(&adc_channels[i], &sequence);
                if (err < 0) {
                    printk("%s: read error (%d)\n", channel_names[i], err);
                    continue;
                }
                int32_t val_mv = (int32_t)buf;
                err = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);
                if (err < 0) {
                    printk("%-11s raw=%4d (mV n/a)\n", channel_names[i], buf);
                } else {
                    printk("%-11s raw=%4d  %4d mV\n", channel_names[i], buf, val_mv);
                }
            }
            printk("----\n");
            last_adc_print = k_uptime_get();
        }

        k_sleep(K_MSEC(10));
    }

    return 0;
}