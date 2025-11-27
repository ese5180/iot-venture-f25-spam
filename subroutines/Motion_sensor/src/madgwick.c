#include <madgwick.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* 全局变量 */
static float beta = 0.1f;
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

void MadgwickInit(float gain)
{
    beta = gain;
    q0 = 1.0f; q1 = q2 = q3 = 0.0f;
}

void MadgwickUpdate(float gx, float gy, float gz,
                    float ax, float ay, float az)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;

    gx *= 0.5f; gy *= 0.5f; gz *= 0.5f;

    float _q0 = q0, _q1 = q1, _q2 = q2, _q3 = q3;

    qDot1 = (-_q1 * gx - _q2 * gy - _q3 * gz);
    qDot2 = (_q0 * gx + _q2 * gz - _q3 * gy);
    qDot3 = (_q0 * gy - _q1 * gz + _q3 * gx);
    qDot4 = (_q0 * gz + _q1 * gy - _q2 * gx);

    recipNorm = ax * ax + ay * ay + az * az;
    if (recipNorm > 0.0f) {
        recipNorm = 1.0f / sqrtf(recipNorm);

        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        s0 = 4.0f * _q0 * (_q2*_q2 + _q3*_q3)
            + 2.0f * ax * (-_q2)
            + 2.0f * ay * (_q1)
            + 2.0f * az * (_q1);

        s1 = 4.0f * _q1 * (_q2*_q2 + _q3*_q3)
            + 2.0f * ax * (_q1)
            + 2.0f * ay * (_q0)
            - 2.0f * az * (_q0);

        s2 = 4.0f * _q2 * (_q1*_q1 + _q3*_q3)
            + 2.0f * ax * (_q0)
            - 2.0f * ay * (_q3)
            + 2.0f * az * (_q2);

        s3 = 4.0f * _q3 * (_q1*_q1 + _q2*_q2)
            - 2.0f * ax * (_q1)
            + 2.0f * ay * (_q2)
            + 2.0f * az * (_q3);

        recipNorm = 1.0f / sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm; s3 *= recipNorm;

        qDot1 -= beta * s0;
        qDot2 -= beta * s1;
        qDot3 -= beta * s2;
        qDot4 -= beta * s3;
    }

    q0 += qDot1 * 0.01f;
    q1 += qDot2 * 0.01f;
    q2 += qDot3 * 0.01f;
    q3 += qDot4 * 0.01f;

    recipNorm = 1.0f / sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 *= recipNorm; q1 *= recipNorm; q2 *= recipNorm; q3 *= recipNorm;
}

void MadgwickGetEuler(float *roll, float *pitch, float *yaw)
{
    *roll  = atan2f(2*(q0*q1 + q2*q3), 1 - 2*(q1*q1 + q2*q2)) * 180.f/M_PI;
    *pitch = asinf (2*(q0*q2 - q3*q1)) * 180.f/M_PI;
    *yaw   = atan2f(2*(q0*q3 + q1*q2), 1 - 2*(q2*q2 + q3*q3)) * 180.f/M_PI;
}
