/* servo_logic.h - pure logic, no Zephyr APIs */

#ifndef SERVO_LOGIC_H_
#define SERVO_LOGIC_H_

#include <stdbool.h>

#define SERVO_MIN_DEG (-90.0f)
#define SERVO_MAX_DEG ( 90.0f)

struct servo_state {
    float current_angle_deg;
};

static inline void servo_state_init(struct servo_state *s)
{
    s->current_angle_deg = 0.0f;
}

/* Clamp to mechanical limits */
float servo_clamp_angle(float angle_deg);

/* Apply one command to the logical state only.
 * Returns 0 on success, -EINVAL on unknown command.
 */
int servo_apply_command_logic(struct servo_state *s, char cmd);

#endif /* SERVO_LOGIC_H_ */
