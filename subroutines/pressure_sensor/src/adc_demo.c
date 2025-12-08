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
#define ADC_NODE         DT_NODELABEL(adc)
#define ADC_CHANNEL_ID_1 1   /* sensor 1 on AIN2 / channel@1 */
#define ADC_CHANNEL_ID_2 3   /* sensor 2 on AIN3 / channel@3 in your overlay */

#define ADC_RESOLUTION   12
#define ADC_GAIN         ADC_GAIN_1
#define ADC_REFERENCE    ADC_REF_INTERNAL
#define ADC_ACQ_TIME     ADC_ACQ_TIME_DEFAULT

static const struct device *adc_dev;
static int16_t sample_buffer1;
static int16_t sample_buffer2;


/* Sensor 1: AIN2 (P0.06), reg = 1 */
static struct adc_channel_cfg channel_cfg1 = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQ_TIME,
    .channel_id       = ADC_CHANNEL_ID_1,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = NRF_SAADC_AIN2,
#endif
};

/* Sensor 2: AIN3 (P0.07), reg = 3 */
static struct adc_channel_cfg channel_cfg2 = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQ_TIME,
    .channel_id       = ADC_CHANNEL_ID_2,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = NRF_SAADC_AIN3,
#endif
};


static struct adc_sequence sequence1 = {
    .channels    = BIT(ADC_CHANNEL_ID_1),
    .buffer      = &sample_buffer1,
    .buffer_size = sizeof(sample_buffer1),
    .resolution  = ADC_RESOLUTION,
};

static struct adc_sequence sequence2 = {
    .channels    = BIT(ADC_CHANNEL_ID_2),
    .buffer      = &sample_buffer2,
    .buffer_size = sizeof(sample_buffer2),
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

    int ret = adc_channel_setup(adc_dev, &channel_cfg1);
    if (ret < 0) {
        LOG_ERR("adc_channel_setup ch1 failed (%d)", ret);
        return ret;
    }

    ret = adc_channel_setup(adc_dev, &channel_cfg2);
    if (ret < 0) {
        LOG_ERR("adc_channel_setup ch2 failed (%d)", ret);
        return ret;
    }

    LOG_INF("ADC demo initialized (2 channels)");
    return 0;
}


/* ============================================
 *              测量 ADC 一次
 * ============================================ */
int adc_demo_read(void)
{
    int ret = adc_read(adc_dev, &sequence1);
    if (ret < 0) {
        LOG_ERR("adc_read ch1 failed (%d)", ret);
        return ret;
    }

    return sample_buffer1;   /* sensor 1 raw value */
}

int adc_demo_read2(void)
{
    int ret1 = adc_read(adc_dev, &sequence2);
    if (ret1 < 0) {
        LOG_ERR("adc_read ch2 failed (%d)", ret1);
        return ret1;
    }

    return sample_buffer2;   /* sensor 2 raw value */
}


#endif /* CONFIG_BOARD_QEMU_CORTEX_M3 */
