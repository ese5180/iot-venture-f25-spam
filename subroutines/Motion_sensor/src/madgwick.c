// madgwick.c
#include <math.h>
#include "madgwick.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ----------------- 内部状态 ----------------- */
static float beta = 0.1f;          /* 算法增益 */
static float q0 = 1.0f, q1 = 0.0f;
static float q2 = 0.0f, q3 = 0.0f;

/* 简单的 1/sqrt(x) 封装 */
static inline float invSqrt(float x)
{
    return 1.0f / sqrtf(x);
}

/* 初始化：设置 beta，并重置四元数 */
void MadgwickInit(float gain)
{
    beta = gain;
    q0 = 1.0f;
    q1 = 0.0f;
    q2 = 0.0f;
    q3 = 0.0f;
}

/* gx, gy, gz:   rad/s（Zephyr LSM6DSO 驱动就是 rad/s）
 * ax, ay, az:   m/s^2（或者任意比例一致的重力单位）
 * dt:           本次更新时间间隔（秒）
 */
void MadgwickUpdate(float gx, float gy, float gz,
                    float ax, float ay, float az,
                    float dt)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3;
    float _4q0, _4q1, _4q2;
    float _8q1, _8q2;
    float q0q0, q1q1, q2q2, q3q3;

    if (dt <= 0.0f) {
        return;
    }

    /* ---------- 1. 由陀螺仪得到四元数变化率 ---------- */
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    /* ---------- 2. 如果有加速度数据，用来校正重力方向 ---------- */
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

        /* 2.1 归一化加速度 */
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        /* 2.2 一堆辅助变量（完全照 Madgwick 原始实现） */
        _2q0 = 2.0f * q0;
        _2q1 = 2.0f * q1;
        _2q2 = 2.0f * q2;
        _2q3 = 2.0f * q3;
        _4q0 = 4.0f * q0;
        _4q1 = 4.0f * q1;
        _4q2 = 4.0f * q2;
        _8q1 = 8.0f * q1;
        _8q2 = 8.0f * q2;
        q0q0 = q0 * q0;
        q1q1 = q1 * q1;
        q2q2 = q2 * q2;
        q3q3 = q3 * q3;

        /* 2.3 梯度下降算法的校正项（原公式） */
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1
           - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3
           - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        /* 2.4 归一化步长 */
        recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        /* 2.5 把校正项叠加到四元数变化率中 */
        qDot1 -= beta * s0;
        qDot2 -= beta * s1;
        qDot3 -= beta * s2;
        qDot4 -= beta * s3;
    }

    /* ---------- 3. 用真实 dt 积分四元数 ---------- */
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    /* ---------- 4. 再归一化一次四元数 ---------- */
    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
}

/* 输出欧拉角（单位：度） */
void MadgwickGetEuler(float *roll, float *pitch, float *yaw)
{
    if (roll) {
        *roll = atan2f(2.0f * (q0*q1 + q2*q3),
                       1.0f - 2.0f * (q1*q1 + q2*q2)) * 180.0f / M_PI;
    }

    if (pitch) {
        float sp = 2.0f * (q0*q2 - q3*q1);
        if (sp > 1.0f)  sp = 1.0f;
        if (sp < -1.0f) sp = -1.0f;
        *pitch = asinf(sp) * 180.0f / M_PI;
    }

    if (yaw) {
        *yaw = atan2f(2.0f * (q0*q3 + q1*q2),
                      1.0f - 2.0f * (q2*q2 + q3*q3)) * 180.0f / M_PI;
    }
}
