#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nrf-saadc.h>

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


/* Channel you want to read: channel2 (reg=2) */
#define ADC_CHANNEL_ID_2   2

/* Buffer for ADC result */
static int16_t sample_buffer;
static int16_t sample_buffer2;

int main(void)
{
    LOG_INF("ADC demo started!");

    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready!");
        return 0;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain             = ADC_GAIN,
        .reference        = ADC_REFERENCE,
        .acquisition_time = ADC_ACQ_TIME,
        .channel_id       = ADC_CHANNEL_ID,
        .input_positive   = NRF_SAADC_AIN2,   // MUST match your overlay!
    };

    /* NEW: second channel config: sensor 2 on AIN3 */
    struct adc_channel_cfg channel_cfg2 = {
        .gain             = ADC_GAIN,
        .reference        = ADC_REFERENCE,
        .acquisition_time = ADC_ACQ_TIME,
        .channel_id       = ADC_CHANNEL_ID_2,
        .input_positive   = NRF_SAADC_AIN3,   // MUST match your overlay!
    };



    int ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret) {
        LOG_ERR("adc_channel_setup failed (%d)", ret);
        return 0;
    }

    ret = adc_channel_setup(adc_dev, &channel_cfg2);
    if (ret) {
        LOG_ERR("adc_channel_setup for ch2 failed (%d)", ret);
        return 0;
    }

    /* ADC sequence config */
    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL_ID),
        .buffer      = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = ADC_RESOLUTION,
    };

    /* NEW: ADC sequence config for sensor 2 */
    struct adc_sequence sequence2 = {
        .channels    = BIT(ADC_CHANNEL_ID_2),
        .buffer      = &sample_buffer2,
        .buffer_size = sizeof(sample_buffer2),
        .resolution  = ADC_RESOLUTION,
    };

    while (1) {
        ret = adc_read(adc_dev, &sequence);
        if (ret) {
            LOG_ERR("adc_read failed (%d)", ret);
        } else {
            LOG_INF("ADC raw value = %d", sample_buffer);
        }

         /* Read second sensor (AIN3) */
        ret = adc_read(adc_dev, &sequence2);
        if (ret) {
            LOG_ERR("adc_read ch2 failed (%d)", ret);
        } else {
            LOG_INF("Sensor 2 (AIN3) raw value = %d", sample_buffer2);
        }

        k_sleep(K_MSEC(500));
    }
}
