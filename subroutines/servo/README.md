# Servo Motor Subroutine

This is the source code for the functionality of the sail winch servo motor. The subroutine in itself is a stand alone Zephyr project, and can be tested individually by isolating it.

## How to call this file in our main app?

Navigate to `src/servo.c`, this is a driver which has all the functions for our servo motor. `servo_run()` is used for the hardware test. Its recommended not to call this, as it has an inifinte loop within. 

## How to run the servo motor seperately?

Go to the root folder of the project. Run the following command:

```west build -p always -b nrf7002dk/nrf5340/cpuapp --sysbuild -d subroutines/servo/build subroutines/servo```

The build folder will be generated inside `subroutines/servo`. Now, to flash the code, run:

```west flash -d subroutines/servo/build```

## How to run the Z-Test?

Go to `subroutines/servo/tests/SERVO_UNIT_TEST` and run the build config:

```west build -p always -b qemu_cortex_m3 . --no-sysbuild```

After that, run this command to perform Z-Test. Run this from the project root (`YOUR_PATH/iot-venture-f25-spam`):

```west twister  -T subroutines/servo/tests/SERVO_UNIT_TEST -p qemu_cortex_m3 -O subroutines/servo/tests/SERVO_UNIT_TEST/twister-out --inline-logs -v```