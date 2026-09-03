#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <stdio.h>

#define ADC_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_channels[] = {
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 0), /* CH1 - PA0  */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 1), /* CH2 - PA1  */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 2), /* CH3 - PA4  */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 3), /* CH4 - PB1  */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 4), /* CH5 - PB11 */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 5), /* CH6 - PB12 */
};

static const char *channel_names[] = {
    "CH1(PA0)", "CH2(PA1)", "CH3(PA4)",
    "CH4(PB1)", "CH5(PB11)", "CH6(PB12)"
};

int main(void)
{
    int err;
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    /* Configure each channel */
    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk("ADC controller for %s not ready\n", channel_names[i]);
            return 0;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk("Could not setup %s (%d)\n", channel_names[i], err);
            return 0;
        }
    }

    while (1) {
        for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
            (void)adc_sequence_init_dt(&adc_channels[i], &sequence);

            err = adc_read_dt(&adc_channels[i], &sequence);
            if (err < 0) {
                printk("Could not read %s (%d)\n", channel_names[i], err);
                continue;
            }

            int32_t val_mv = buf;
            err = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);

            if (err < 0) {
                printk("%s: raw=%d (mV conversion not available)\n",
                       channel_names[i], buf);
            } else {
                printk("%s: raw=%d, %d mV\n", channel_names[i], buf, val_mv);
            }
        }

        printk("----\n");
        k_sleep(K_MSEC(1000));
    }

    return 0;
}