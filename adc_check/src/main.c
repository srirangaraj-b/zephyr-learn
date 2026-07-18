#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>

#define ADC_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_chan =
    ADC_DT_SPEC_GET(ADC_NODE);

int main(void)
{
    int16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    adc_channel_setup_dt(&adc_chan);

    while (1) {
        adc_sequence_init_dt(&adc_chan, &sequence);

        if (adc_read(adc_chan.dev, &sequence) == 0) {
            printk("ADC = %d\n", buf);
        }

        k_sleep(K_MSEC(500));
    }
}