#include "adc_demo.h"
#include <errno.h>   /* for -ENODEV */

/* ------------------------------------------------------------------
 * For unit tests on qemu_cortex_m3, there is no real ADC hardware.
 * Provide a stub implementation so the code compiles and tests can
 * still verify higher-level logic.
 * ------------------------------------------------------------------ */
#ifdef CONFIG_BOARD_QEMU_CORTEX_M3

int adc_demo_init(void)
{
    /* No ADC device on qemu: behave as "device not present" */
    return -ENODEV;
}

int adc_demo_read(void)
{
    /* No reading possible without hardware */
    return -ENODEV;
}

#else  /* !CONFIG_BOARD_QEMU_CORTEX_M3 : real hardware path */

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

static struct adc_channel_cfg channel_cfg = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQ_TIME,
    .channel_id       = ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = NRF_SAADC_AIN2,
#endif
};

static struct adc_sequence sequence = {
    .channels    = BIT(ADC_CHANNEL_ID),
    .buffer      = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution  = ADC_RESOLUTION,
};

/* ============================================
 *              初始化 ADC
 * ============================================ */
int adc_demo_init(void)
{
    adc_dev = DEVICE_DT_GET(ADC_NODE);
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    int ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret < 0) {
        LOG_ERR("adc_channel_setup failed (%d)", ret);
        return ret;
    }

    LOG_INF("ADC demo initialized");
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

    return sample_buffer;   /* 返回原始 ADC 数值 */
}

#endif /* CONFIG_BOARD_QEMU_CORTEX_M3 */
