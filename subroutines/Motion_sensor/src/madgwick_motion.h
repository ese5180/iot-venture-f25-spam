#ifndef MADGWICK_MOTION_H_
#define MADGWICK_MOTION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化第二套 Madgwick 滤波器（给第二个 IMU 用） */
void MadgwickMotionInit(float gain);

/* 
 * gx, gy, gz:   rad/s（Zephyr LSM6DSO 驱动就是 rad/s）
 * ax, ay, az:   m/s^2（或者任意比例一致的重力单位）
 * dt:           本次更新时间间隔（秒）
 */
void MadgwickMotionUpdate(float gx, float gy, float gz,
                          float ax, float ay, float az,
                          float dt);

/* 输出欧拉角（单位：度）——来自“第二套”滤波状态 */
void MadgwickMotionGetEuler(float *roll, float *pitch, float *yaw);

#ifdef __cplusplus
}
#endif

#endif /* MADGWICK_MOTION_H_ */
