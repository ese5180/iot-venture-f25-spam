#include "adc_demo.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nrf-saadc.h>

LOG_MODULE_REGISTER(adc_demo, LOG_LEVEL_INF);

/* =======================
 *   ADC 配置参数
 * ======================= */
#define ADC_NODE        DT_NODELABEL(adc)
#define ADC_CHANNEL_ID  1
#define ADC_RESOLUTION  12
#define ADC_GAIN        ADC_GAIN_1
#define ADC_REFERENCE   ADC_REF_INTERNAL
#define ADC_ACQ_TIME    ADC_ACQ_TIME_DEFAULT



static const struct device *adc_dev;
static int16_t sample_buffer;

/* ADC 采集序列 */
static struct adc_sequence sequence = {
    .channels    = BIT(ADC_CHANNEL_ID),
    .buffer      = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution  = ADC_RESOLUTION,
};

/* ============================================
 *          ADC 初始化函数
 * ============================================ */
int adc_demo_init(void)
{
    adc_dev = DEVICE_DT_GET(ADC_NODE);

    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready!");
        return -ENODEV;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain             = ADC_GAIN,
        .reference        = ADC_REFERENCE,
        .acquisition_time = ADC_ACQ_TIME,
        .channel_id       = ADC_CHANNEL_ID,
        .input_positive   = NRF_SAADC_AIN2,
    };

    int ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret < 0) {
        LOG_ERR("adc_channel_setup failed (%d)", ret);
        return ret;
    }

    LOG_INF("ADC initialized successfully");
    return 0;
}

/* ============================================
 *              测量 ADC 一次
 * ============================================ */
int adc_demo_read(void)
{
    int ret = adc_read(adc_dev, &sequence);
    if (ret < 0) {
        LOG_ERR("adc_read failed (%d)", ret);
        return ret;
    }

    return sample_buffer;   // 返回原始 ADC 数值
}
