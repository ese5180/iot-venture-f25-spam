// #include "imu.h"
// #include <madgwick.h>
// #include <math.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>

// // LOG_MODULE_REGISTER(imu_logic, LOG_LEVEL_INF);

// // #ifndef M_PI
// // #define M_PI 3.14159265358979323846
// // #endif

// // static const struct device *imu_dev;

// // static float roll_offset = 0, pitch_offset = 0, yaw_offset = 0;
// // static bool imu_ready = false;

// // /* =====================================
// //  *     初始化（包含校准）
// //  * ===================================== */
// // int imu_init(void) {
// //   imu_dev = DEVICE_DT_GET_ANY(st_lsm6dso);

// //   if (!imu_dev || !device_is_ready(imu_dev)) {
// //     LOG_ERR("LSM6DSO not ready!");
// //     return -ENODEV;
// //   }

// //   MadgwickInit(0.1f);

// //   LOG_INF("IMU calibration started...");

// //   float roll, pitch, yaw;
// //   int samples = 0;

// //   for (int i = 0; i < 100; i++) {
// //     struct sensor_value accel[3], gyro[3];

// //     sensor_sample_fetch(imu_dev);
// //     sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
// //     sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);

// //     float ax = sensor_value_to_double(&accel[0]);
// //     float ay = sensor_value_to_double(&accel[1]);
// //     float az = sensor_value_to_double(&accel[2]);

// //     float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
// //     float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
// //     float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));

// //     MadgwickUpdate(gx, gy, gz, ax, ay, az);
// //     MadgwickGetEuler(&roll, &pitch, &yaw);

// //     roll_offset += roll;
// //     pitch_offset += pitch;
// //     yaw_offset += yaw;

// //     samples++;
// //     k_sleep(K_MSEC(100));
// //   }

// //   roll_offset /= samples;
// //   pitch_offset /= samples;
// //   yaw_offset /= samples;

// //   LOG_INF("Calibration done: R=%.2f P=%.2f Y=%.2f", (double)roll_offset,
// //           (double)pitch_offset, (double)yaw_offset);

// //   imu_ready = true;
// //   return 0;
// // }

// // int imu_calibrate(int ms) {
// //   if (!imu_ready) {
// //     LOG_ERR("imu_calibrate() called before imu_init()");
// //     return -EFAULT;
// //   }

// //   LOG_INF("IMU calibration started (%d ms)...", ms);

// //   float roll, pitch, yaw;
// //   int samples = 0;

// //   roll_offset = pitch_offset = yaw_offset = 0;

// //   int loops = ms / 10; // 每 10ms 一次

// //   for (int i = 0; i < loops; i++) {

// //     struct sensor_value accel[3], gyro[3];

// //     sensor_sample_fetch(imu_dev);
// //     sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
// //     sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);

// //     float ax = sensor_value_to_double(&accel[0]);
// //     float ay = sensor_value_to_double(&accel[1]);
// //     float az = sensor_value_to_double(&accel[2]);

// //     float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
// //     float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
// //     float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));

// //     MadgwickUpdate(gx, gy, gz, ax, ay, az);
// //     MadgwickGetEuler(&roll, &pitch, &yaw);

// //     roll_offset += roll;
// //     pitch_offset += pitch;
// //     yaw_offset += yaw;

// //     samples++;
// //     k_sleep(K_MSEC(10));
// //   }

// //   roll_offset /= samples;
// //   pitch_offset /= samples;
// //   yaw_offset /= samples;

// //   LOG_INF("Calibration done: R=%.2f P=%.2f Y=%.2f", (double)roll_offset,
// //           (double)pitch_offset, (double)yaw_offset);

// //   return 0;
// // }

// // /* =====================================
// //  *     每一次调用 → 更新一次姿态
// //  * ===================================== */
// // int imu_update(struct imu_angles *out) {
// //   if (!imu_ready) {
// //     LOG_ERR("imu_update() called before imu_init()");
// //     return -EFAULT;
// //   }

// //   struct sensor_value accel[3], gyro[3];

// //   sensor_sample_fetch(imu_dev);
// //   sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
// //   sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);

// //   float ax = sensor_value_to_double(&accel[0]);
// //   float ay = sensor_value_to_double(&accel[1]);
// //   float az = sensor_value_to_double(&accel[2]);

// //   float gx = (float)(sensor_value_to_double(&gyro[0]) * (M_PI / 180.0));
// //   float gy = (float)(sensor_value_to_double(&gyro[1]) * (M_PI / 180.0));
// //   float gz = (float)(sensor_value_to_double(&gyro[2]) * (M_PI / 180.0));

// //   float roll, pitch, yaw;

// //   MadgwickUpdate(gx, gy, gz, ax, ay, az);
// //   MadgwickGetEuler(&roll, &pitch, &yaw);

// //   /* 关键：这里自动减去 offset —— main 不需要知道 offset */
// //   out->roll = roll - roll_offset;
// //   out->pitch = pitch - pitch_offset;
// //   out->yaw = yaw - yaw_offset;

// //   return 0;
// // }

// // void imu_reset_reference(void) {
// //   float roll, pitch, yaw;

// //   /* 获取当前姿态 */
// //   MadgwickGetEuler(&roll, &pitch, &yaw);

// //   /* 设为新参考点 */
// //   roll_offset = roll;
// //   pitch_offset = pitch;
// //   yaw_offset = yaw;

// //   LOG_INF("IMU reference reset: R=%.2f P=%.2f Y=%.2f", (double)roll_offset,
// //           (double)pitch_offset, (double)yaw_offset);
// // }



// // LOG_MODULE_REGISTER(imu_subsys, LOG_LEVEL_INF);

// // /* --------- 模块内部状态 --------- */

// // static const struct device *imu_dev;
// // static bool imu_ready = false;

// // /* Gyro bias（rad/s） */
// // static float gbx = 0.0f, gby = 0.0f, gbz = 0.0f;

// // /* 姿态基准角（deg）—— 由 calibration 得到 */
// // static float ref_roll  = 0.0f;
// // static float ref_pitch = 0.0f;
// // static float ref_yaw   = 0.0f;

// // /* 最近一次计算好的姿态角（已经减去参考） */
// // static struct imu_angles last_angles = {0};

// // /* 上一次更新时间（ms） */
// // static int64_t last_ts_ms = 0;

// // /* 线程相关 */
// // #define IMU_THREAD_STACK_SIZE  1024
// // #define IMU_THREAD_PRIORITY    6

// // K_THREAD_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);
// // static struct k_thread imu_thread_data;

// // /* --------- IMU 线程：10ms 周期跑 Madgwick --------- */

// // static void imu_thread_fn(void *p1, void *p2, void *p3)
// // {
// //     ARG_UNUSED(p1);
// //     ARG_UNUSED(p2);
// //     ARG_UNUSED(p3);

// //     struct sensor_value accel[3];
// //     struct sensor_value gyro[3];

// //     while (1) {

// //         if (!imu_ready) {
// //             k_sleep(K_MSEC(10));
// //             continue;
// //         }

// //         int64_t now_ms = k_uptime_get();
// //         float dt = (now_ms - last_ts_ms) / 1000.0f;
// //         last_ts_ms = now_ms;

// //         /* 防止偶尔 dt 太大/太小 */
// //         if (dt <= 0.0f || dt > 0.1f) {
// //             dt = 0.01f;
// //         }

// //         if (sensor_sample_fetch(imu_dev) < 0) {
// //             LOG_WRN("sensor_sample_fetch failed in imu_thread");
// //             k_sleep(K_MSEC(10));
// //             continue;
// //         }

// //         if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
// //             LOG_WRN("get accel failed in imu_thread");
// //             k_sleep(K_MSEC(10));
// //             continue;
// //         }

// //         if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
// //             LOG_WRN("get gyro failed in imu_thread");
// //             k_sleep(K_MSEC(10));
// //             continue;
// //         }

// //         float ax = sensor_value_to_double(&accel[0]);
// //         float ay = sensor_value_to_double(&accel[1]);
// //         float az = sensor_value_to_double(&accel[2]);

// //         float gx = sensor_value_to_double(&gyro[0]) - gbx;  // rad/s
// //         float gy = sensor_value_to_double(&gyro[1]) - gby;
// //         float gz = sensor_value_to_double(&gyro[2]) - gbz;

// //         float roll, pitch, yaw;

// //         MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
// //         MadgwickGetEuler(&roll, &pitch, &yaw);

// //         /* 做“当前 - 参考” 的减法，得到相对角度 */
// //         last_angles.roll  = roll  - ref_roll;
// //         last_angles.pitch = pitch - ref_pitch;
// //         last_angles.yaw   = yaw   - ref_yaw;

// //         /* 10 ms 周期 */
// //         k_sleep(K_MSEC(10));
// //     }
// // }

// // /* --------- 对外 API 实现 --------- */

// // int imu_init(void)
// // {
// //     imu_dev = DEVICE_DT_GET_ANY(st_lsm6dso);
// //     if (!imu_dev || !device_is_ready(imu_dev)) {
// //         LOG_ERR("IMU (LSM6DSO) not ready");
// //         imu_ready = false;
// //         return -ENODEV;
// //     }

// //     /* 1) 初始化 Madgwick */
// //     MadgwickInit(0.1f);

// //     /* 2) 3 秒陀螺仪零偏标定（板子静止） */
// //     LOG_INF("Calibrating gyro bias, keep IMU still...");

// //     struct sensor_value gyro[3];
// //     const int calib_samples = 300;
// //     gbx = gby = gbz = 0.0f;

// //     for (int i = 0; i < calib_samples; i++) {
// //         if (sensor_sample_fetch(imu_dev) < 0) {
// //             continue;
// //         }
// //         if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
// //             continue;
// //         }

// //         gbx += sensor_value_to_double(&gyro[0]);
// //         gby += sensor_value_to_double(&gyro[1]);
// //         gbz += sensor_value_to_double(&gyro[2]);

// //         k_sleep(K_MSEC(10));
// //     }

// //     gbx /= calib_samples;
// //     gby /= calib_samples;
// //     gbz /= calib_samples;

// //     LOG_INF("Gyro bias: bx=%.6f  by=%.6f  bz=%.6f",
// //             gbx, gby, gbz);

// //     /* 3) 标定参考姿态：让 Madgwick 收敛到当前静止姿态 */
// //     LOG_INF("Calibrating reference pose...");

// //     struct sensor_value accel[3];
// //     float roll, pitch, yaw;
// //     const int orient_samples = 200;

// //     ref_roll = ref_pitch = ref_yaw = 0.0f;
// //     last_ts_ms = k_uptime_get();

// //     for (int i = 0; i < orient_samples; i++) {

// //         int64_t now_ms = k_uptime_get();
// //         float dt = (now_ms - last_ts_ms) / 1000.0f;
// //         last_ts_ms = now_ms;
// //         if (dt <= 0.0f || dt > 0.1f) {
// //             dt = 0.01f;
// //         }

// //         if (sensor_sample_fetch(imu_dev) < 0) {
// //             continue;
// //         }

// //         if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
// //             continue;
// //         }
// //         if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
// //             continue;
// //         }

// //         float ax = sensor_value_to_double(&accel[0]);
// //         float ay = sensor_value_to_double(&accel[1]);
// //         float az = sensor_value_to_double(&accel[2]);

// //         float gx = sensor_value_to_double(&gyro[0]) - gbx;
// //         float gy = sensor_value_to_double(&gyro[1]) - gby;
// //         float gz = sensor_value_to_double(&gyro[2]) - gbz;

// //         MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
// //         MadgwickGetEuler(&roll, &pitch, &yaw);

// //         ref_roll  += roll;
// //         ref_pitch += pitch;
// //         ref_yaw   += yaw;

// //         k_sleep(K_MSEC(10));
// //     }

// //     ref_roll  /= orient_samples;
// //     ref_pitch /= orient_samples;
// //     ref_yaw   /= orient_samples;

// //     LOG_INF("Reference pose: R=%.2f  P=%.2f  Y=%.2f",
// //             ref_roll, ref_pitch, ref_yaw);

// //     /* 参考姿态对应的相对角度就是 0 */
// //     last_angles.roll  = 0.0f;
// //     last_angles.pitch = 0.0f;
// //     last_angles.yaw   = 0.0f;

// //     /* 4) 启动 IMU 线程，后面 Madgwick 就在这个线程里以 10ms 周期跑 */
// //     imu_ready = true;
// //     last_ts_ms = k_uptime_get();

// //     k_thread_create(&imu_thread_data,
// //                     imu_thread_stack,
// //                     K_THREAD_STACK_SIZEOF(imu_thread_stack),
// //                     imu_thread_fn,
// //                     NULL, NULL, NULL,
// //                     IMU_THREAD_PRIORITY,
// //                     0,
// //                     K_NO_WAIT);

// //     LOG_INF("IMU thread started");

// //     return 0;
// // }


// // int imu_update(struct imu_angles *angles)
// // {
// //     if (!imu_ready) {
// //         return -EAGAIN;
// //     }

// //     if (angles) {
// //         /* 直接把最近一次线程算好的结果拷贝出去 */
// //         *angles = last_angles;
// //     }

// //     return 0;
// // }


// LOG_MODULE_REGISTER(imu_subsys, LOG_LEVEL_INF);

// /* --------- 模块内部状态 --------- */

// static const struct device *imu_dev;
// static bool imu_ready = false;

// /* Gyro bias（rad/s） */
// static float gbx = 0.0f, gby = 0.0f, gbz = 0.0f;

// /* 姿态基准角（deg）—— 由姿态校准得到 */
// static float ref_roll  = 0.0f;
// static float ref_pitch = 0.0f;
// static float ref_yaw   = 0.0f;

// /* 最近一次计算好的姿态角（已经减去参考） */
// static struct imu_angles last_angles = {0};

// /* 上一次更新时间（ms） */
// static int64_t last_ts_ms = 0;

// /* 线程相关 */
// #define IMU_THREAD_STACK_SIZE  1024
// #define IMU_THREAD_PRIORITY    6

// K_THREAD_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);
// static struct k_thread imu_thread_data;

// /* ===========================================================
//  *          IMU 线程：10ms 周期跑 Madgwick
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

//         /* 注意：Zephyr gyro 单位是 rad/s */
//         float gx = sensor_value_to_double(&gyro[0]) - gbx;
//         float gy = sensor_value_to_double(&gyro[1]) - gby;
//         float gz = sensor_value_to_double(&gyro[2]) - gbz;

//         float roll, pitch, yaw;

//         MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
//         MadgwickGetEuler(&roll, &pitch, &yaw);

//         /* 做“当前 - 参考” 的减法，得到相对角度 */
//         last_angles.roll  = roll  - ref_roll;
//         last_angles.pitch = pitch - ref_pitch;
//         last_angles.yaw   = yaw   - ref_yaw;

//         /* 10 ms 周期 */
//         k_sleep(K_MSEC(10));
//     }
// }

// /* ===========================================================
//  *                      对外 API
//  * ===========================================================*/

// /* imu_init()
//  * 1) 找设备
//  * 2) 初始化 Madgwick
//  * 3) 3 秒 gyro 零偏校准
//  * 4) 10 秒姿态校准 → 得到 ref_roll/pitch/yaw
//  * 5) 启动 IMU 线程（10ms 周期更新）
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
//     MadgwickInit(0.1f);

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

//     /* 3) 10 秒姿态校准：让 Madgwick 收敛，并把这段时间的平均姿势当作参考 */
//     LOG_INF("Calibrating initial pose for ~10s, keep IMU in desired reference posture...");

//     struct sensor_value accel[3];
//     float roll = 0, pitch = 0, yaw = 0;

//     const int orient_samples = 1000;   // 1000 * 10ms ≈ 10s
//     ref_roll  = 0.0f;
//     ref_pitch = 0.0f;
//     ref_yaw   = 0.0f;

//     last_ts_ms = k_uptime_get();

//     for (int i = 0; i < orient_samples; i++) {

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

//         ref_roll  += roll;
//         ref_pitch += pitch;
//         ref_yaw   += yaw;

//         k_sleep(K_MSEC(10));
//     }

//     ref_roll  /= orient_samples;
//     ref_pitch /= orient_samples;
//     ref_yaw   /= orient_samples;

//     LOG_INF("Initial reference pose: R=%.2f  P=%.2f  Y=%.2f",
//             ref_roll, ref_pitch, ref_yaw);

//     /* 参考姿态对应的相对角度就是 0 */
//     last_angles.roll  = 0.0f;
//     last_angles.pitch = 0.0f;
//     last_angles.yaw   = 0.0f;

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

// /* 每次被 main 调用，只是把线程算好的 last_angles 拷贝出去 */
// int imu_update(struct imu_angles *angles)
// {
//     if (!imu_ready) {
//         return -EAGAIN;
//     }

//     if (angles) {
//         *angles = last_angles;
//     }

//     return 0;
// }

// void imu_reset_reference(void)
// {
//     if (!imu_ready) {
//         return;
//     }

//     float roll, pitch, yaw;

//     /* 从 Madgwick 当前四元数拿到“绝对姿态” */
//     MadgwickGetEuler(&roll, &pitch, &yaw);

//     /* 把当前绝对角度作为新的参考姿态 */
//     ref_roll  = roll;
//     ref_pitch = pitch;
//     ref_yaw   = yaw;

//     /* 相对角度清零 */
//     last_angles.roll  = 0.0f;
//     last_angles.pitch = 0.0f;
//     last_angles.yaw   = 0.0f;

//     LOG_INF("IMU reference reset: R=%.2f  P=%.2f  Y=%.2f",
//             ref_roll, ref_pitch, ref_yaw);
// }



#include "imu.h"
#include <madgwick.h>
#include <math.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/*
 *  这个版本的 imu.c 只负责：
 *  - 找到 LSM6DSO 设备
 *  - 做 3s 的陀螺仪零偏校准（gyro bias）
 *  - 做约 10s 的 Madgwick 预热，让姿态稳定
 *  - 启动一个 10ms 周期的线程，不断更新绝对姿态角
 *
 *  对外：imu_update() 返回的是 **绝对 roll/pitch/yaw（不减参考）**
 *  “当前 - 参考”的比较逻辑，请在 main.c 里自己做。
 */

LOG_MODULE_REGISTER(imu_subsys, LOG_LEVEL_INF);

/* --------- 模块内部状态 --------- */

static const struct device *imu_dev;
static bool imu_ready = false;

/* Gyro bias（rad/s），从 3 秒静止数据中估计 */
static float gbx = 0.0f, gby = 0.0f, gbz = 0.0f;

/* 最近一次计算好的绝对姿态角（度） */
static struct imu_angles last_angles = {0};

/* 上一次更新时间（ms），用于算 dt */
static int64_t last_ts_ms = 0;

/* 线程相关 */
#define IMU_THREAD_STACK_SIZE  1024
#define IMU_THREAD_PRIORITY    6

K_THREAD_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);
static struct k_thread imu_thread_data;

/* ===========================================================
 *          IMU 线程：10ms 周期跑 Madgwick（绝对角）
 * ===========================================================*/
static void imu_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    while (1) {

        if (!imu_ready) {
            k_sleep(K_MSEC(10));
            continue;
        }

        int64_t now_ms = k_uptime_get();
        float dt = (now_ms - last_ts_ms) / 1000.0f;
        last_ts_ms = now_ms;

        /* 防止偶尔 dt 太大/太小 */
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 0.01f;
        }

        if (sensor_sample_fetch(imu_dev) < 0) {
            LOG_WRN("sensor_sample_fetch failed in imu_thread");
            k_sleep(K_MSEC(10));
            continue;
        }

        if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
            LOG_WRN("get accel failed in imu_thread");
            k_sleep(K_MSEC(10));
            continue;
        }

        if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
            LOG_WRN("get gyro failed in imu_thread");
            k_sleep(K_MSEC(10));
            continue;
        }

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        /* 注意：Zephyr LSM6DSO 的 gyro 输出是 rad/s */
        float gx = sensor_value_to_double(&gyro[0]) - gbx;
        float gy = sensor_value_to_double(&gyro[1]) - gby;
        float gz = sensor_value_to_double(&gyro[2]) - gbz;

        float roll, pitch, yaw;

        MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
        MadgwickGetEuler(&roll, &pitch, &yaw);   // 这里返回的是角度（度）

        /* 直接保存“绝对姿态” */
        last_angles.roll  = roll;
        last_angles.pitch = pitch;
        last_angles.yaw   = yaw;

        /* 10 ms 周期 */
        k_sleep(K_MSEC(10));
    }
}

/* ===========================================================
 *                      对外 API
 * ===========================================================*/

/*
 * imu_init()
 * 1) 找设备
 * 2) 初始化 Madgwick
 * 3) 3 秒 gyro 零偏校准（板子静止）
 * 4) 10 秒 Madgwick 预热（板子保持参考姿势）
 * 5) 启动 IMU 线程（10ms 周期更新绝对角度）
 */
int imu_init(void)
{
    imu_dev = DEVICE_DT_GET_ANY(st_lsm6dso);
    if (!imu_dev || !device_is_ready(imu_dev)) {
        LOG_ERR("IMU (LSM6DSO) not ready");
        imu_ready = false;
        return -ENODEV;
    }

    /* 1) 初始化 Madgwick 滤波器 */
    MadgwickInit(0.1f);   // 保持你之前的参数

    /* 2) 3 秒陀螺仪零偏标定（板子静止） */
    LOG_INF("Calibrating gyro bias, keep IMU still (about 3s)...");

    struct sensor_value gyro[3];
    const int calib_samples = 300;    // 300 * 10ms ≈ 3s
    gbx = gby = gbz = 0.0f;

    for (int i = 0; i < calib_samples; i++) {
        if (sensor_sample_fetch(imu_dev) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }
        if (sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        gbx += sensor_value_to_double(&gyro[0]);
        gby += sensor_value_to_double(&gyro[1]);
        gbz += sensor_value_to_double(&gyro[2]);

        k_sleep(K_MSEC(10));
    }

    gbx /= calib_samples;
    gby /= calib_samples;
    gbz /= calib_samples;

    LOG_INF("Gyro bias: bx=%.6f  by=%.6f  bz=%.6f", gbx, gby, gbz);

    /* 3) 10 秒 Madgwick 预热：让滤波器收敛到当前姿势 */
    LOG_INF("Warming up Madgwick for ~10s, keep IMU in desired reference posture...");

    struct sensor_value accel[3];
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;

    const int warmup_samples = 1000;   // 1000 * 10ms ≈ 10s

    last_ts_ms = k_uptime_get();

    for (int i = 0; i < warmup_samples; i++) {

        int64_t now_ms = k_uptime_get();
        float dt = (now_ms - last_ts_ms) / 1000.0f;
        last_ts_ms = now_ms;
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 0.01f;
        }

        if (sensor_sample_fetch(imu_dev) < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0 ||
            sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ,  gyro)  < 0) {
            k_sleep(K_MSEC(10));
            continue;
        }

        float ax = sensor_value_to_double(&accel[0]);
        float ay = sensor_value_to_double(&accel[1]);
        float az = sensor_value_to_double(&accel[2]);

        float gx = sensor_value_to_double(&gyro[0]) - gbx;
        float gy = sensor_value_to_double(&gyro[1]) - gby;
        float gz = sensor_value_to_double(&gyro[2]) - gbz;

        MadgwickUpdate(gx, gy, gz, ax, ay, az, dt);
        MadgwickGetEuler(&roll, &pitch, &yaw);

        k_sleep(K_MSEC(10));
    }

    LOG_INF("Warmup done, last pose (absolute): R=%.2f  P=%.2f  Y=%.2f",
            roll, pitch, yaw);

    /* 把当前绝对姿态作为初始输出（main 第一次 imu_update 基本能读到类似的值） */
    last_angles.roll  = roll;
    last_angles.pitch = pitch;
    last_angles.yaw   = yaw;

    /* 4) 启动 IMU 线程，后面 Madgwick 就在这个线程里以 10ms 周期跑 */
    imu_ready   = true;
    last_ts_ms  = k_uptime_get();

    k_thread_create(&imu_thread_data,
                    imu_thread_stack,
                    K_THREAD_STACK_SIZEOF(imu_thread_stack),
                    imu_thread_fn,
                    NULL, NULL, NULL,
                    IMU_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    LOG_INF("IMU thread started");

    return 0;
}

/* 每次被 main 调用，只是把线程算好的 **绝对角度** 拷贝出去 */
int imu_update(struct imu_angles *angles)
{
    if (!imu_ready) {
        return -EAGAIN;
    }

    if (angles) {
        *angles = last_angles;   // 绝对 roll/pitch/yaw
    }

    return 0;
}
