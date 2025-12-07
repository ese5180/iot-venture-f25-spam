// // imu.h
// #ifndef IMU_H_
// #define IMU_H_

// #include <zephyr/drivers/sensor.h>

// struct imu_angles {
//   float roll;
//   float pitch;
//   float yaw;
// };

// int imu_init(void);
// int imu_update(struct imu_angles *angles);
// void imu_reset_reference(void);


// #endif


/* imu.h - 双 IMU 接口 */

#ifndef IMU_H_
#define IMU_H_

#ifdef __cplusplus
extern "C" {
#endif

struct imu_angles {
    float roll;
    float pitch;
    float yaw;
};

/* 初始化两个 IMU（主 + motion），以及两套 Madgwick */
int imu_init(void);

/* 读取主 IMU（挂在 I2C1 上的那个）的姿态角（绝对角度） */
int imu_update_main(struct imu_angles *angles);

/* 读取第二个 IMU（motion，用 I2C2）的姿态角（绝对角度） */
int imu_update_motion(struct imu_angles *angles);

/* 兼容旧代码：imu_update() 等价于 imu_update_main() */
static inline int imu_update(struct imu_angles *angles)
{
    return imu_update_main(angles);
}

#ifdef __cplusplus
}
#endif

#endif /* IMU_H_ */
