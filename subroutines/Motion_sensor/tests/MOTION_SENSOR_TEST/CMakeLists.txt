# tests/MOTION_SENSOR_TEST/CMakeLists.txt

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(motion_sensor_test)

# 添加你的 motion sensor 模块源码（让 ztest 能调用 imu_init / imu_update）
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../subroutines/Motion_sensor/src/imu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../subroutines/Motion_sensor/src/madgwick.c
)

# 包含头文件路径
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../subroutines/Motion_sensor/src
)