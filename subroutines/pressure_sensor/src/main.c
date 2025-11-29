#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>

LOG_MODULE_REGISTER(adc_demo, LOG_LEVEL_INF);

/* ADC device name for nRF53 SAADC */
#define ADC_NODE DT_NODELABEL(adc)
static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);

/* Channel you want to read: channel1 (reg=1) */
#define ADC_CHANNEL_ID   1
#define ADC_RESOLUTION   12
#define ADC_GAIN         ADC_GAIN_1
#define ADC_REFERENCE    ADC_REF_INTERNAL
#define ADC_ACQ_TIME     ADC_ACQ_TIME_DEFAULT

/* Buffer for ADC result */
static int16_t sample_buffer;

void main(void)
{
    LOG_INF("ADC demo started!");

    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready!");
        return;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain             = ADC_GAIN,
        .reference        = ADC_REFERENCE,
        .acquisition_time = ADC_ACQ_TIME,
        .channel_id       = ADC_CHANNEL_ID,
        .input_positive   = NRF_SAADC_AIN2,   // MUST match your overlay!
    };

    int ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret) {
        LOG_ERR("adc_channel_setup failed (%d)", ret);
        return;
    }

    /* ADC sequence config */
    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL_ID),
        .buffer      = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = ADC_RESOLUTION,
    };

    while (1) {
        ret = adc_read(adc_dev, &sequence);
        if (ret) {
            LOG_ERR("adc_read failed (%d)", ret);
        } else {
            LOG_INF("ADC raw value = %d", sample_buffer);
        }

        k_sleep(K_MSEC(500));
    }
}
