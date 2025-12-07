// #include "imu.h"
// #include <madgwick.h>
// #include <math.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>

// LOG_MODULE_REGISTER(imu_subsys, LOG_LEVEL_INF);

// /* --------- 模块内部状态 --------- */

// static const struct device *imu_dev;
// static bool imu_ready = false;

// /* Gyro bias（rad/s），从 3 秒静止数据中估计 */
// static float gbx = 0.0f, gby = 0.0f, gbz = 0.0f;

// /* 最近一次计算好的绝对姿态角（度） */
// static struct imu_angles last_angles = {0};

// /* 上一次更新时间（ms），用于算 dt */
// static int64_t last_ts_ms = 0;

// /* 线程相关 */
// #define IMU_THREAD_STACK_SIZE  1024
// #define IMU_THREAD_PRIORITY    6

// K_THREAD_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);
// static struct k_thread imu_thread_data;

// /* ===========================================================
//  *          IMU 线程：10ms 周期跑 Madgwick（绝对角）
//  * ===========================================================*/
// static void imu_thread_fn(void *p1, void *p2, void *p3)
// {
//     ARG_UNUSED(p1);
//     ARG_UNUSED(p2);
//     ARG_UNUSED(p3);

//     struct sensor_value accel[3];
//     struct sensor_value gyro[3];

//     while (1) {

//         if (!imu_ready) {
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         int64_t now_ms = k_uptime_get();
//         float dt = (now_ms - last_ts_ms) / 1000.0f;
//         last_ts_ms = now_ms;

//         /* 防止偶尔 dt 太大/太小 */
//         if (dt <= 0.0f || dt > 0.1f) {
//             dt = 0.01f;
//         }

//         if (sensor_sample_fetch(imu_dev) < 0) {
//             LOG_WRN("sensor_sample_fetch failed in imu_thread");
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
//             LOG_WRN("get accel failed in imu_thread");
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
//             LOG_WRN("get gyro failed in imu_thread");
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         float ax = sensor_value_to_double(&accel[0]);
//         float ay = sensor_value_to_double(&accel[1]);
//         float az = sensor_value_to_double(&accel[2]);

//         /* 注意：Zephyr LSM6DSO 的 gyro 输出是 rad/s */
//         float gx = sensor_value_to_double(&gyro[0]) - gbx;
//         float gy = sensor_value_to_double(&gyro[1]) - gby;
//         float gz = sensor_value_to_double(&gyro[2]) - gbz;

//         float roll, pitch, yaw;

//         MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
//         MadgwickGetEuler(&roll, &pitch, &yaw);   // 这里返回的是角度（度）

//         /* 直接保存“绝对姿态” */
//         last_angles.roll  = roll;
//         last_angles.pitch = pitch;
//         last_angles.yaw   = yaw;

//         /* 10 ms 周期 */
//         k_sleep(K_MSEC(10));
//     }
// }

// /* ===========================================================
//  *                      对外 API
//  * ===========================================================*/

// /*
//  * imu_init()
//  * 1) 找设备
//  * 2) 初始化 Madgwick
//  * 3) 3 秒 gyro 零偏校准（板子静止）
//  * 4) 10 秒 Madgwick 预热（板子保持参考姿势）
//  * 5) 启动 IMU 线程（10ms 周期更新绝对角度）
//  */
// int imu_init(void)
// {
//     imu_dev = DEVICE_DT_GET_ANY(st_lsm6dso);
//     if (!imu_dev || !device_is_ready(imu_dev)) {
//         LOG_ERR("IMU (LSM6DSO) not ready");
//         imu_ready = false;
//         return -ENODEV;
//     }

//     /* 1) 初始化 Madgwick 滤波器 */
//     MadgwickInit(0.1f);   // 保持你之前的参数

//     /* 2) 3 秒陀螺仪零偏标定（板子静止） */
//     LOG_INF("Calibrating gyro bias, keep IMU still (about 3s)...");

//     struct sensor_value gyro[3];
//     const int calib_samples = 300;    // 300 * 10ms ≈ 3s
//     gbx = gby = gbz = 0.0f;

//     for (int i = 0; i < calib_samples; i++) {
//         if (sensor_sample_fetch(imu_dev) < 0) {
//             k_sleep(K_MSEC(10));
//             continue;
//         }
//         if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         gbx += sensor_value_to_double(&gyro[0]);
//         gby += sensor_value_to_double(&gyro[1]);
//         gbz += sensor_value_to_double(&gyro[2]);

//         k_sleep(K_MSEC(10));
//     }

//     gbx /= calib_samples;
//     gby /= calib_samples;
//     gbz /= calib_samples;

//     LOG_INF("Gyro bias: bx=%.6f  by=%.6f  bz=%.6f", gbx, gby, gbz);

//     /* 3) 10 秒 Madgwick 预热：让滤波器收敛到当前姿势 */
//     LOG_INF("Warming up Madgwick for ~10s, keep IMU in desired reference posture...");

//     struct sensor_value accel[3];
//     float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;

//     const int warmup_samples = 1000;   // 1000 * 10ms ≈ 10s

//     last_ts_ms = k_uptime_get();

//     for (int i = 0; i < warmup_samples; i++) {

//         int64_t now_ms = k_uptime_get();
//         float dt = (now_ms - last_ts_ms) / 1000.0f;
//         last_ts_ms = now_ms;
//         if (dt <= 0.0f || dt > 0.1f) {
//             dt = 0.01f;
//         }

//         if (sensor_sample_fetch(imu_dev) < 0) {
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0 ||
//             sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro)  < 0) {
//             k_sleep(K_MSEC(10));
//             continue;
//         }

//         float ax = sensor_value_to_double(&accel[0]);
//         float ay = sensor_value_to_double(&accel[1]);
//         float az = sensor_value_to_double(&accel[2]);

//         float gx = sensor_value_to_double(&gyro[0]) - gbx;
//         float gy = sensor_value_to_double(&gyro[1]) - gby;
//         float gz = sensor_value_to_double(&gyro[2]) - gbz;

//         MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
//         MadgwickGetEuler(&roll, &pitch, &yaw);

//         k_sleep(K_MSEC(10));
//     }

//     LOG_INF("Warmup done, last pose (absolute): R=%.2f  P=%.2f  Y=%.2f",
//             roll, pitch, yaw);

//     /* 把当前绝对姿态作为初始输出（main 第一次 imu_update 基本能读到类似的值） */
//     last_angles.roll  = roll;
//     last_angles.pitch = pitch;
//     last_angles.yaw   = yaw;

//     /* 4) 启动 IMU 线程，后面 Madgwick 就在这个线程里以 10ms 周期跑 */
//     imu_ready   = true;
//     last_ts_ms  = k_uptime_get();

//     k_thread_create(&imu_thread_data,
//                     imu_thread_stack,
//                     K_THREAD_STACK_SIZEOF(imu_thread_stack),
//                     imu_thread_fn,
//                     NULL, NULL, NULL,
//                     IMU_THREAD_PRIORITY,
//                     0,
//                     K_NO_WAIT);

//     LOG_INF("IMU thread started");

//     return 0;
// }

// /* 每次被 main 调用，只是把线程算好的 **绝对角度** 拷贝出去 */
// int imu_update(struct imu_angles *angles)
// {
//     if (!imu_ready) {
//         return -EAGAIN;
//     }

//     if (angles) {
//         *angles = last_angles;   // 绝对 roll/pitch/yaw
//     }

//     return 0;
// }



/* imu.c - 双 IMU + 双 Madgwick 版本 */

#include "imu.h"
#include <madgwick.h>          /* 主 IMU 的 Madgwick */
#include <madgwick_motion.h>   /* 第二个 IMU 的 Madgwick（你复制改名的那份） */

#include <math.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_subsys, LOG_LEVEL_INF);

#define IMU_MAIN_NODE   DT_NODELABEL(lsm6dso0)
#define IMU_MOTION_NODE DT_NODELABEL(lsm6dso1)

/* --------- 模块内部状态 --------- */

/* 两个 IMU 的 device 指针 */
static const struct device *imu_main_dev   = DEVICE_DT_GET(IMU_MAIN_NODE);
static const struct device *imu_motion_dev = DEVICE_DT_GET(IMU_MOTION_NODE);

/* 就绪标志 */
static bool imu_main_ready   = false;
static bool imu_motion_ready = false;

/* 每个 IMU 一套 gyro bias（rad/s） */
static float gbx_main = 0.0f, gby_main = 0.0f, gbz_main = 0.0f;
static float gbx_motion = 0.0f, gby_motion = 0.0f, gbz_motion = 0.0f;

/* 最近一次计算好的绝对姿态角（度） */
static struct imu_angles last_main_angles   = {0};
static struct imu_angles last_motion_angles = {0};

/* 上一次更新时间（ms），用于算 dt（分别给两个 IMU 用） */
static int64_t last_main_ts_ms   = 0;
static int64_t last_motion_ts_ms = 0;

/* 线程相关：一个线程里轮流更新两个 IMU */
#define IMU_THREAD_STACK_SIZE  1024
#define IMU_THREAD_PRIORITY    6

K_THREAD_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);
static struct k_thread imu_thread_data;

/* ===========================================================
 *          IMU 线程：10ms 周期轮询两个 IMU（绝对角）
 * ===========================================================*/
static void imu_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    while (1) {

        /* ---------- 主 IMU 更新 ---------- */
        if (imu_main_ready) {

            int64_t now_ms = k_uptime_get();
            float dt = (now_ms - last_main_ts_ms) / 1000.0f;
            last_main_ts_ms = now_ms;

            if (dt <= 0.0f || dt > 0.1f) {
                dt = 0.01f;
            }

            if (sensor_sample_fetch(imu_main_dev) < 0) {
                LOG_WRN("sensor_sample_fetch MAIN failed");
            } else if (sensor_channel_get(imu_main_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
                LOG_WRN("get accel MAIN failed");
            } else if (sensor_channel_get(imu_main_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
                LOG_WRN("get gyro MAIN failed");
            } else {
                float ax = sensor_value_to_double(&accel[0]);
                float ay = sensor_value_to_double(&accel[1]);
                float az = sensor_value_to_double(&accel[2]);

                float gx = sensor_value_to_double(&gyro[0]) - gbx_main;
                float gy = sensor_value_to_double(&gyro[1]) - gby_main;
                float gz = sensor_value_to_double(&gyro[2]) - gbz_main;

                float roll, pitch, yaw;
                MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
                MadgwickGetEuler(&roll, &pitch, &yaw);

                last_main_angles.roll  = roll;
                last_main_angles.pitch = pitch;
                last_main_angles.yaw   = yaw;
            }
        }

        /* ---------- 第二个 IMU（motion）更新 ---------- */
        if (imu_motion_ready) {

            int64_t now_ms = k_uptime_get();
            float dt = (now_ms - last_motion_ts_ms) / 1000.0f;
            last_motion_ts_ms = now_ms;

            if (dt <= 0.0f || dt > 0.1f) {
                dt = 0.01f;
            }

            if (sensor_sample_fetch(imu_motion_dev) < 0) {
                LOG_WRN("sensor_sample_fetch MOTION failed");
            } else if (sensor_channel_get(imu_motion_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
                LOG_WRN("get accel MOTION failed");
            } else if (sensor_channel_get(imu_motion_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
                LOG_WRN("get gyro MOTION failed");
            } else {
                float ax = sensor_value_to_double(&accel[0]);
                float ay = sensor_value_to_double(&accel[1]);
                float az = sensor_value_to_double(&accel[2]);

                float gx = sensor_value_to_double(&gyro[0]) - gbx_motion;
                float gy = sensor_value_to_double(&gyro[1]) - gby_motion;
                float gz = sensor_value_to_double(&gyro[2]) - gbz_motion;

                float roll, pitch, yaw;
                MadgwickMotionUpdate(gx, gy, gz, ax, ay, az, dt);
                MadgwickMotionGetEuler(&roll, &pitch, &yaw);

                last_motion_angles.roll  = roll;
                last_motion_angles.pitch = pitch;
                last_motion_angles.yaw   = yaw;
            }
        }

        /* 10 ms 周期 */
        k_sleep(K_MSEC(10));
    }
}

/* ===========================================================
 *                 内部帮助函数：标定一个 IMU
 * ===========================================================*/

/* 对某个 IMU 做 3 秒 gyro 零偏标定，返回 bias */
static int calibrate_gyro_bias(const struct device *dev,
                               float *bx, float *by, float *bz)
{
    struct sensor_value gyro[3];
    const int calib_samples = 300;    /* 300 * 10ms ≈ 3s */

    *bx = *by = *bz = 0.0f;

    for (int i = 0; i < calib_samples; i++) {
        if (sensor_sample_fetch(dev) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }
        if (sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        *bx += sensor_value_to_double(&gyro[0]);
        *by += sensor_value_to_double(&gyro[1]);
        *bz += sensor_value_to_double(&gyro[2]);

        k_sleep(K_MSEC(10));
    }

    *bx /= calib_samples;
    *by /= calib_samples;
    *bz /= calib_samples;

    return 0;
}

/* 对某个 IMU 做约 10 秒 Madgwick 预热，让滤波稳定 */
static int warmup_madgwick_main(const struct device *dev)
{
    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    const int warmup_samples = 1000;   /* 1000 * 10ms ≈ 10s */

    last_main_ts_ms = k_uptime_get();

    for (int i = 0; i < warmup_samples; i++) {

        int64_t now_ms = k_uptime_get();
        float dt = (now_ms - last_main_ts_ms) / 1000.0f;
        last_main_ts_ms = now_ms;
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 0.01f;
        }

        if (sensor_sample_fetch(dev) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0 ||
            sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ,  gyro)  < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        float gx = sensor_value_to_double(&gyro[0]) - gbx_main;
        float gy = sensor_value_to_double(&gyro[1]) - gby_main;
        float gz = sensor_value_to_double(&gyro[2]) - gbz_main;

        MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
        MadgwickGetEuler(&roll, &pitch, &yaw);

        k_sleep(K_MSEC(10));
    }

    last_main_angles.roll  = roll;
    last_main_angles.pitch = pitch;
    last_main_angles.yaw   = yaw;

    LOG_INF("MAIN warmup done: R=%.2f P=%.2f Y=%.2f",
            roll, pitch, yaw);

    return 0;
}

static int warmup_madgwick_motion(const struct device *dev)
{
    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    const int warmup_samples = 1000;   /* 1000 * 10ms ≈ 10s */

    last_motion_ts_ms = k_uptime_get();

    for (int i = 0; i < warmup_samples; i++) {

        int64_t now_ms = k_uptime_get();
        float dt = (now_ms - last_motion_ts_ms) / 1000.0f;
        last_motion_ts_ms = now_ms;
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 0.01f;
        }

        if (sensor_sample_fetch(dev) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0 ||
            sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ,  gyro)  < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        float gx = sensor_value_to_double(&gyro[0]) - gbx_motion;
        float gy = sensor_value_to_double(&gyro[1]) - gby_motion;
        float gz = sensor_value_to_double(&gyro[2]) - gbz_motion;

        MadgwickMotionUpdate(gx, gy, gz, ax, ay, az, dt);
        MadgwickMotionGetEuler(&roll, &pitch, &yaw);

        k_sleep(K_MSEC(10));
    }

    last_motion_angles.roll  = roll;
    last_motion_angles.pitch = pitch;
    last_motion_angles.yaw   = yaw;

    LOG_INF("MOTION warmup done: R=%.2f P=%.2f Y=%.2f",
            roll, pitch, yaw);

    return 0;
}

/* ===========================================================
 *                      对外 API
 * ===========================================================*/

/*
 * imu_init()
 * 1) 检查两个 IMU 是否 ready
 * 2) 初始化两套 Madgwick
 * 3) 各做 3 秒 gyro 零偏标定
 * 4) 各做 10 秒 Madgwick 预热
 * 5) 启动线程，10ms 周期更新两路绝对姿态角
 */
int imu_init(void)
{
    if (!device_is_ready(imu_main_dev)) {
        LOG_ERR("MAIN IMU (lsm6dso0) not ready");
        return -ENODEV;
    }
    if (!device_is_ready(imu_motion_dev)) {
        LOG_ERR("MOTION IMU (lsm6dso1) not ready");
        return -ENODEV;
    }

    /* 1) 初始化两个滤波器 */
    MadgwickInit(0.1f);
    MadgwickMotionInit(0.1f);

    /* 2) gyro 零偏标定 */
    LOG_INF("Calibrating MAIN gyro bias ~3s, keep still...");
    calibrate_gyro_bias(imu_main_dev, &gbx_main, &gby_main, &gbz_main);
    // LOG_INF("MAIN gyro bias: bx=%.6f by=%.6f bz=%.6f",
    //         gbx_main, gby_main, gbz_main);

    LOG_INF("Calibrating MOTION gyro bias ~3s, keep still...");
    calibrate_gyro_bias(imu_motion_dev, &gbx_motion, &gby_motion, &gbz_motion);
    // LOG_INF("MOTION gyro bias: bx=%.6f by=%.6f bz=%.6f",
    //         gbx_motion, gby_motion, gbz_motion);

    /* 3) 预热 Madgwick（板子摆在你希望的参考姿势） */
    LOG_INF("Warming up MAIN Madgwick ~10s...");
    warmup_madgwick_main(imu_main_dev);

    LOG_INF("Warming up MOTION Madgwick ~10s...");
    warmup_madgwick_motion(imu_motion_dev);

    /* 4) 标记 ready，启动线程 */
    imu_main_ready   = true;
    imu_motion_ready = true;

    k_thread_create(&imu_thread_data,
                    imu_thread_stack,
                    K_THREAD_STACK_SIZEOF(imu_thread_stack),
                    imu_thread_fn,
                    NULL, NULL, NULL,
                    IMU_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    LOG_INF("IMU thread started (MAIN + MOTION)");

    return 0;
}

/* main IMU：返回最近一次绝对姿态角 */
int imu_update_main(struct imu_angles *angles)
{
    if (!imu_main_ready) {
        return -EAGAIN;
    }
    if (angles) {
        *angles = last_main_angles;
    }
    return 0;
}

/* motion IMU：返回最近一次绝对姿态角 */
int imu_update_motion(struct imu_angles *angles)
{
    if (!imu_motion_ready) {
        return -EAGAIN;
    }
    if (angles) {
        *angles = last_motion_angles;
    }
    return 0;
}
