#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "imu.h"

ZTEST(motion_sensor, test_imu_init)
{
    int ret = imu_init();
    zassert_equal(ret, 0, "imu_init failed, ret=%d", ret);
}

ZTEST(motion_sensor, test_imu_update_valid)
{
    struct imu_angles ang;
    int ret = imu_update(&ang);
    zassert_equal(ret, 0, "imu_update failed");

    /* 检查 roll/pitch/yaw 是否是浮点数范围内 */
    zassert_true(ang.roll  > -360 && ang.roll  < 360, "roll invalid");
    zassert_true(ang.pitch > -360 && ang.pitch < 360, "pitch invalid");
    zassert_true(ang.yaw   > -360 && ang.yaw   < 360, "yaw invalid");
}

ZTEST_SUITE(motion_sensor, NULL, NULL, NULL, NULL, NULL);