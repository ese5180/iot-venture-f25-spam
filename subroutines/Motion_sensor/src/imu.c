#include "imu.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <madgwick.h>
#include <math.h>

LOG_MODULE_REGISTER(imu_logic, LOG_LEVEL_INF);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const struct device *imu_dev;

static float roll_offset = 0, pitch_offset = 0, yaw_offset = 0;
static bool imu_ready = false;

/* =====================================
 *     初始化（包含校准）
 * ===================================== */
int imu_init(void)
{
    imu_dev = DEVICE_DT_GET_ANY(st_lsm6dso);

    if (!imu_dev || !device_is_ready(imu_dev)) {
        LOG_ERR("LSM6DSO not ready!");
        return -ENODEV;
    }

    MadgwickInit(0.1f);

    LOG_INF("IMU calibration started...");

    float roll, pitch, yaw;
    int samples = 0;

    for (int i = 0; i < 100; i++) {
        struct sensor_value accel[3], gyro[3];

        sensor_sample_fetch(imu_dev);
        sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
        sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro);

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
        float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
        float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));


        MadgwickUpdate(gx, gy, gz, ax, ay, az);
        MadgwickGetEuler(&roll, &pitch, &yaw);

        roll_offset  += roll;
        pitch_offset += pitch;
        yaw_offset   += yaw;

        samples++;
        k_sleep(K_MSEC(100));
    }

    roll_offset  /= samples;
    pitch_offset /= samples;
    yaw_offset   /= samples;

    LOG_INF("Calibration done: R=%.2f P=%.2f Y=%.2f", (double)roll_offset, (double)pitch_offset, (double)yaw_offset);

    imu_ready = true;
    return 0;
}


int imu_calibrate(int ms)
{
    if (!imu_ready) {
        LOG_ERR("imu_calibrate() called before imu_init()");
        return -EFAULT;
    }

    LOG_INF("IMU calibration started (%d ms)...", ms);

    float roll, pitch, yaw;
    int samples = 0;

    roll_offset = pitch_offset = yaw_offset = 0;

    int loops = ms / 10;   // 每 10ms 一次

    for (int i = 0; i < loops; i++) {

        struct sensor_value accel[3], gyro[3];

        sensor_sample_fetch(imu_dev);
        sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
        sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro);

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
        float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
        float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));

        MadgwickUpdate(gx, gy, gz, ax, ay, az);
        MadgwickGetEuler(&roll, &pitch, &yaw);

        roll_offset  += roll;
        pitch_offset += pitch;
        yaw_offset   += yaw;

        samples++;
        k_sleep(K_MSEC(10));
    }

    roll_offset  /= samples;
    pitch_offset /= samples;
    yaw_offset   /= samples;

    LOG_INF("Calibration done: R=%.2f P=%.2f Y=%.2f", (double)roll_offset, (double)pitch_offset, (double)yaw_offset);

    return 0;
}


/* =====================================
 *     每一次调用 → 更新一次姿态
 * ===================================== */
int imu_update(struct imu_angles *out)
{
    if (!imu_ready) {
        LOG_ERR("imu_update() called before imu_init()");
        return -EFAULT;
    }

    struct sensor_value accel[3], gyro[3];

    sensor_sample_fetch(imu_dev);
    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro);

    float ax = sensor_value_to_double(&accel[0]);
    float ay = sensor_value_to_double(&accel[1]);
    float az = sensor_value_to_double(&accel[2]);

    float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
    float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
    float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));

    float roll, pitch, yaw;

    MadgwickUpdate(gx, gy, gz, ax, ay, az);
    MadgwickGetEuler(&roll, &pitch, &yaw);

    /* 关键：这里自动减去 offset —— main 不需要知道 offset */
    out->roll  = roll  - roll_offset;
    out->pitch = pitch - pitch_offset;
    out->yaw   = yaw   - yaw_offset;

    return 0;
}

void imu_reset_reference(void)
{
    float roll, pitch, yaw;

    /* 获取当前姿态 */
    MadgwickGetEuler(&roll, &pitch, &yaw);

    /* 设为新参考点 */
    roll_offset  = roll;
    pitch_offset = pitch;
    yaw_offset   = yaw;

    LOG_INF("IMU reference reset: R=%.2f P=%.2f Y=%.2f",
            (double)roll_offset,
            (double)pitch_offset,
            (double)yaw_offset);
}