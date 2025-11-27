/* servo_logic.h - pure servo command/angle logic (no Zephyr APIs) */
#ifndef SERVO_LOGIC_H_
#define SERVO_LOGIC_H_

#include <stdbool.h>

/* Total mechanical travel the mapping is based on.
 * Use the same value you used when deriving ns_per_deg in your
 * PWM mapping. If your old tested code used ~1890°, keep that.
 */
#define SERVO_TOTAL_TRAVEL_DEG 1890.0f   /* or 1260.0f if you follow the spec strictly */

#define SERVO_MIN_DEG (-SERVO_TOTAL_TRAVEL_DEG / 2.0f)  /* ≈ -945° */
#define SERVO_MAX_DEG ( SERVO_TOTAL_TRAVEL_DEG / 2.0f)  /* ≈ +945° */


struct servo_state {
    float current_angle_deg;
};

/* Initialize logical state to center */
static inline void servo_state_init(struct servo_state *s)
{
    s->current_angle_deg = 0.0f;
}

/* Clamp an angle to the allowed range */
float servo_clamp_angle(float angle_deg);

/* Apply one command to the logical state only.
 *  '1' -> +90 deg
 *  '2' -> -90 deg
 *  '0', 'c', 'C' -> 0 deg
 *
 * Returns 0 on success, -EINVAL on unknown command.
 */
int servo_apply_command_logic(struct servo_state *s, char cmd);

#endif /* SERVO_LOGIC_H_ */
