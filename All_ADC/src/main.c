#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#define NUM_CHANNELS 6

static const struct adc_dt_spec adc_ch[NUM_CHANNELS] = {
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),  /* INP5  PA8  */
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),  /* INP10 PA9  */
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),  /* INP11 PA10 */
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3),  /* INP13 PA12 */
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 4),  /* INP16 PF3  */
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 5),  /* INP7  PG15 */
};

/* Calibration:
 * Raw = 2007 --> 3.3 V
 * Raw = 2935 --> 5.0 V
 */
static int32_t calibrated_mv(uint16_t raw)
{
    return 3300 + ((int32_t)(raw - 2007) * 1700) / (2935 - 2007);
}

int main(void)
{
    int ret;
    uint32_t buf;
    uint16_t raw;
    int32_t voltage_mv;

    struct adc_sequence seq = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    int i;

    /* Configure ADC channels */
    for (i = 0; i < NUM_CHANNELS; i++) {
        ret = adc_channel_setup_dt(&adc_ch[i]);
        if (ret < 0) {
            printk("Channel %d setup failed: %d\n", i, ret);
            return ret;
        }
    }

    while (1) {
        for (i = 0; i < NUM_CHANNELS; i++) {

            adc_sequence_init_dt(&adc_ch[i], &seq);

            ret = adc_read_dt(&adc_ch[i], &seq);
            if (ret < 0) {
                printk("Channel %d read failed: %d\n", i, ret);
                continue;
            }

            raw = (uint16_t)(buf & 0xFFFF);

            /* Apply calibration */
            voltage_mv = calibrated_mv(raw);

            printk("CH%d : %.3f V\n", i, voltage_mv / 1000.0);
        }

        printk("\n");
        k_sleep(K_MSEC(500));
    }

    return 0;
}