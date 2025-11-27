// imu.h
#ifndef IMU_H_
#define IMU_H_

#include <zephyr/drivers/sensor.h>

struct imu_angles {
    float roll;
    float pitch;
    float yaw;
};

int imu_init(void);
int imu_calibrate(int ms);
int imu_update(struct imu_angles *out);

#endif