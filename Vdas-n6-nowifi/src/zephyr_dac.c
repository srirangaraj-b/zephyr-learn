#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "drivers_dac.h"
#include "calibration_config.h"

#define AD5694_I2C_ADDR         0x0C
#define AD5694_CMD_WRITE_UPDATE 0x30

static const uint8_t ad5694_channel_addr[4] = { 0x01, 0x02, 0x04, 0x08 };

#define DAQ_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec scl = GPIO_DT_SPEC_GET_BY_IDX(DAQ_NODE, gpios, 0);
static const struct gpio_dt_spec sda = GPIO_DT_SPEC_GET_BY_IDX(DAQ_NODE, gpios, 1);

static bool gpio_initialized = false;
static float current_dac_voltage[4] = {0.0f, 0.0f, 0.0f, 0.0f};

static K_MUTEX_DEFINE(dac_bus_mutex);

static inline void i2c_delay(void) { k_busy_wait(5); }
static inline void bb_scl_high(void) { gpio_pin_set_dt(&scl, 1); }
static inline void bb_scl_low(void) { gpio_pin_set_dt(&scl, 0); }
static inline void bb_sda_high(void) { gpio_pin_set_dt(&sda, 1); }
static inline void bb_sda_low(void) { gpio_pin_set_dt(&sda, 0); }

static void bb_i2c_start(void)
{
    bb_sda_high();
    bb_scl_high();
    i2c_delay();
    bb_sda_low();
    i2c_delay();
    bb_scl_low();
    i2c_delay();
}

static void bb_i2c_stop(void)
{
    bb_sda_low();
    i2c_delay();
    bb_scl_high();
    i2c_delay();
    bb_sda_high();
    i2c_delay();
}

static int bb_i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        if (data & BIT(i)) {
            bb_sda_high();
        } else {
            bb_sda_low();
        }
        i2c_delay();
        bb_scl_high();
        i2c_delay();
        bb_scl_low();
        i2c_delay();
    }

    bb_sda_high();
    i2c_delay();
    bb_scl_high();
    i2c_delay();

    gpio_pin_configure_dt(&sda, GPIO_INPUT);
    int sda_level = gpio_pin_get_dt(&sda);
    bb_scl_low();
    i2c_delay();

    gpio_pin_configure_dt(&sda, GPIO_OUTPUT | GPIO_PULL_UP);
    bb_sda_high();

    return (sda_level == 0) ? 0 : -1;
}

static int bb_i2c_init(void)
{
    if (!device_is_ready(scl.port) || !device_is_ready(sda.port)) {
        return -ENODEV;
    }
    gpio_pin_configure_dt(&scl, GPIO_OUTPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&sda, GPIO_OUTPUT | GPIO_PULL_UP);
    bb_scl_high();
    bb_sda_high();
    i2c_delay();
    gpio_initialized = true;
    return 0;
}

static int ad5694_write(uint8_t channel, uint16_t code)
{
    if (!gpio_initialized || channel > 3) {
        return -EINVAL;
    }

    uint16_t dac_data = (uint16_t)(code << 4);

    k_mutex_lock(&dac_bus_mutex, K_FOREVER);
    bb_i2c_start();

    if (bb_i2c_write_byte((AD5694_I2C_ADDR << 1) | 0) != 0) {
        bb_i2c_stop();
        k_mutex_unlock(&dac_bus_mutex);
        return -EIO;
    }

    uint8_t cmd_byte = AD5694_CMD_WRITE_UPDATE | ad5694_channel_addr[channel];
    if (bb_i2c_write_byte(cmd_byte) != 0 ||
        bb_i2c_write_byte((uint8_t)((dac_data >> 8) & 0xFF)) != 0 ||
        bb_i2c_write_byte((uint8_t)(dac_data & 0xFF)) != 0) {
        bb_i2c_stop();
        k_mutex_unlock(&dac_bus_mutex);
        return -EIO;
    }

    bb_i2c_stop();
    k_mutex_unlock(&dac_bus_mutex);
    return 0;
}

void DAC_Init(void)
{
    if (bb_i2c_init() != 0) return;
    for (uint8_t ch = 0; ch < DAQ_NUM_DAC_CHANNELS; ch++) {
        ad5694_write(ch, 0);
        current_dac_voltage[ch] = 0.0f;
    }
}

int DAC_SetRawCode(uint8_t channel, uint16_t code)
{
    if (channel >= DAQ_NUM_DAC_CHANNELS) return -EINVAL;
    if (code > 4095) code = 4095;
    
    int ret = ad5694_write(channel, code);
    if (ret == 0) {
        current_dac_voltage[channel] = ((float)code / 4095.0f) * 5.0f;
    }
    return ret;
}

int DAC_SetVoltage(uint8_t channel, float voltage)
{
    if (channel >= DAQ_NUM_DAC_CHANNELS) return -EINVAL;

    uint16_t code = CALIB_DacVoltageToCode(channel, voltage);
    int ret = ad5694_write(channel, code);
    if (ret == 0) {
        current_dac_voltage[channel] = voltage;
    }
    return ret;
}

float DAC_GetVoltage(uint8_t channel)
{
    if (channel >= DAQ_NUM_DAC_CHANNELS) return 0.0f;
    return current_dac_voltage[channel];
}

int DAC_SetCurrent(uint8_t channel, float current_mA)
{
    if (channel >= DAQ_NUM_DAC_CHANNELS) return -EINVAL;
    float voltage = CALIB_DacCurrentToVoltage(channel, current_mA);
    return DAC_SetVoltage(channel, voltage);
}

float DAC_GetCurrent(uint8_t channel)
{
    if (channel >= DAQ_NUM_DAC_CHANNELS) return 0.0f;
    return CALIB_DacVoltageToCurrent(channel, current_dac_voltage[channel]);
}

int DAC_SetSafe(void)
{
    int ret = 0;
    for (uint8_t ch = 0; ch < DAQ_NUM_DAC_CHANNELS; ch++) {
        int r = DAC_SetVoltage(ch, 0.0f);
        if (r != 0) ret = r;
    }
    return ret;
}

const struct device *DAC_GetDevice(void)
{
    return NULL;
}