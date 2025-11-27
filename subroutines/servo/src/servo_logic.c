#include "servo_logic.h"
#include <errno.h>

float servo_clamp_angle(float angle_deg)
{
    if (angle_deg < SERVO_MIN_DEG) return SERVO_MIN_DEG;
    if (angle_deg > SERVO_MAX_DEG) return SERVO_MAX_DEG;
    return angle_deg;
}

int servo_apply_command_logic(struct servo_state *s, char cmd)
{
    float next = s->current_angle_deg;

    switch (cmd) {
    case '1':
        next += 90.0f;
        break;
    case '2':
        next -= 90.0f;
        break;
    case '0':
    case 'c':
    case 'C':
        next = 0.0f;
        break;
    default:
        return -EINVAL;
    }

    s->current_angle_deg = servo_clamp_angle(next);
    return 0;
}
