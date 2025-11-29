#include <zephyr/ztest.h>
#include "adc_demo.h"

/* Test that adc_demo_init() behaves sensibly on both real HW and qemu */
ZTEST(pressure_sensor_test_suite, test_adc_init)
{
    int ret = adc_demo_init();

    /* On real hardware we expect 0, on qemu it's usually -ENODEV.
     * Accept both so the test is portable. */
    zassert_true((ret == 0) || (ret == -ENODEV),
                 "adc_demo_init returned unexpected value %d", ret);
}

/* Test that adc_demo_read() returns a valid 12-bit raw value when ADC exists */
ZTEST(pressure_sensor_test_suite, test_adc_read_valid_range)
{
    int ret = adc_demo_init();

    /* If there is no ADC device (e.g. qemu), skip this test */
    if (ret == -ENODEV) {
        ztest_test_skip();
    }

    zassert_equal(0, ret, "adc_demo_init failed, ret=%d", ret);

    int value = adc_demo_read();
    /* adc_demo.h says 0–4095 is the valid raw range for 12-bit ADC */
    zassert_true(value >= 0 && value <= 4095,
                 "ADC raw value out of range: %d", value);
}

/* Register the suite and hook up (optional) fixtures */
ZTEST_SUITE(pressure_sensor_test_suite, NULL, NULL, NULL, NULL, NULL);
