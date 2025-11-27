#ifndef MADGWICK_H
#define MADGWICK_H

void MadgwickInit(float gain);
void MadgwickUpdate(float gx, float gy, float gz,
                    float ax, float ay, float az);
void MadgwickGetEuler(float *roll, float *pitch, float *yaw);

#endif
