# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iot_venture_f25_spam_root)

target_sources(app PRIVATE
    src/main.c
    subroutines/servo/src/servo.c
    subroutines/ble_nus/src/ble_nus.c
    subroutines/servo/src/servo_logic.c 
    subroutines/Motion_sensor/src/imu.c
    subroutines/Motion_sensor/src/madgwick.c
    subroutines/pressure_sensor/src/adc_demo.c
)

target_include_directories(app PRIVATE
    subroutines/servo/src
    subroutines/ble_nus/src
    subroutines/Motion_sensor/src
    subroutines/pressure_sensor/src
)
