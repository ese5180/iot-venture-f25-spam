#include <zephyr/ztest.h>
#include "servo_logic.h"

ZTEST(servo_logic_test_suite, test_servo_clamp_basic)
{
    /* Center stays center */
    zassert_equal(servo_clamp_angle(0.0f), 0.0f, "Center should stay 0");

    /* Below min clamps to SERVO_MIN_DEG */
    float below_min = SERVO_MIN_DEG - 200.0f;
    zassert_equal(servo_clamp_angle(below_min),
                  SERVO_MIN_DEG,
                  "Too low must clamp to SERVO_MIN_DEG");

    /* Above max clamps to SERVO_MAX_DEG */
    float above_max = SERVO_MAX_DEG + 200.0f;
    zassert_equal(servo_clamp_angle(above_max),
                  SERVO_MAX_DEG,
                  "Too high must clamp to SERVO_MAX_DEG");
}

ZTEST(servo_logic_test_suite, test_servo_commands_positive_negative)
{
    struct servo_state s;
    servo_state_init(&s);

    /* '1' -> +90 deg from 0 */
    zassert_ok(servo_apply_command_logic(&s, '1'));
    zassert_equal(s.current_angle_deg, 90.0f, "Should move to +90 deg");

    /* Another '1' -> +180 deg (no clamp yet, large travel) */
    zassert_ok(servo_apply_command_logic(&s, '1'));
    zassert_equal(s.current_angle_deg, 180.0f, "Should move to +180 deg");

    /* '2' steps back down */
    zassert_ok(servo_apply_command_logic(&s, '2'));
    zassert_equal(s.current_angle_deg, 90.0f, "Should move back to +90 deg");

    zassert_ok(servo_apply_command_logic(&s, '2'));
    zassert_equal(s.current_angle_deg, 0.0f, "Should return to 0 deg");

    /* Near max: stepping over should clamp at SERVO_MAX_DEG */
    s.current_angle_deg = SERVO_MAX_DEG - 45.0f;
    zassert_ok(servo_apply_command_logic(&s, '1'));
    zassert_equal(s.current_angle_deg,
                  SERVO_MAX_DEG,
                  "Step past max should clamp at SERVO_MAX_DEG");

    /* Near min: stepping under should clamp at SERVO_MIN_DEG */
    s.current_angle_deg = SERVO_MIN_DEG + 45.0f;
    zassert_ok(servo_apply_command_logic(&s, '2'));
    zassert_equal(s.current_angle_deg,
                  SERVO_MIN_DEG,
                  "Step past min should clamp at SERVO_MIN_DEG");
}

ZTEST(servo_logic_test_suite, test_servo_commands_zero_and_invalid)
{
    struct servo_state s;
    servo_state_init(&s);

    /* '0' centers from any angle */
    s.current_angle_deg = 45.0f;
    zassert_ok(servo_apply_command_logic(&s, '0'));
    zassert_equal(s.current_angle_deg, 0.0f, "Command 0 should center");

    /* Invalid command must not change angle */
    s.current_angle_deg = 10.0f;
    int ret = servo_apply_command_logic(&s, 'x');
    zassert_equal(ret, -EINVAL, "Invalid command must return -EINVAL");
    zassert_equal(s.current_angle_deg, 10.0f, "Angle must remain unchanged");
}

ZTEST_SUITE(servo_logic_test_suite, NULL, NULL, NULL, NULL, NULL);
