/* servo.h - shared servo control API */
#ifndef SERVO_H_
#define SERVO_H_

/* Initialize PWM and center the servo. Call once at startup. */
int servo_init(void);

/* Apply one command:
 *  '1' -> +90 deg
 *  '2' -> -90 deg
 *  '0', 'c', 'C' -> center
 */
int servo_apply_command(char cmd);

/* Original standalone demo using UART (still works as before). */
int servo_run(void);

#endif /* SERVO_H_ */
