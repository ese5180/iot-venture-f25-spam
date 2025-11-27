#include "servo.h"
#include "servo_logic.h"

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(servo_ctrl, LOG_LEVEL_INF);

/* ===== Devicetree setup ===== */

#define SERVO_NODE DT_NODELABEL(servo)
BUILD_ASSERT(DT_NODE_HAS_STATUS(SERVO_NODE, okay),
             "Devicetree node 'servo' not found or disabled");

static const struct pwm_dt_spec servo_pwm = PWM_DT_SPEC_GET(SERVO_NODE);
static const uint32_t servo_min_pulse_ns = DT_PROP(SERVO_NODE, min_pulse);
static const uint32_t servo_max_pulse_ns = DT_PROP(SERVO_NODE, max_pulse);

#define SERVO_MIN_DEG (-90.0f)
#define SERVO_MAX_DEG ( 90.0f)

static float current_angle_deg = 0.0f;
static struct servo_state g_state;

/* ----- internal helpers ----- */

static float clamp_angle(float angle_deg, bool *clamped)
{
    float capped = angle_deg;

    if (capped < SERVO_MIN_DEG) {
        capped = SERVO_MIN_DEG;
        if (clamped) *clamped = true;
    } else if (capped > SERVO_MAX_DEG) {
        capped = SERVO_MAX_DEG;
        if (clamped) *clamped = true;
    }

    return capped;
}

static int servo_set_angle(float angle_deg)
{
    bool limited = false;
    float target = clamp_angle(angle_deg, &limited);

    if (!device_is_ready(servo_pwm.dev)) {
        LOG_ERR("Servo PWM device not ready");
        return -ENODEV;
    }

    float span_deg = SERVO_MAX_DEG - SERVO_MIN_DEG;   /* 180 */
    float normalized = (target - SERVO_MIN_DEG) / span_deg;  /* 0..1 */

    uint32_t pulse_ns = servo_min_pulse_ns +
        (uint32_t)((servo_max_pulse_ns - servo_min_pulse_ns) * normalized);

    int err = pwm_set_pulse_dt(&servo_pwm, pulse_ns);
    if (err) {
        LOG_ERR("Failed to set PWM pulse: %d", err);
        return err;
    }

    current_angle_deg = target;

    if (limited) {
        LOG_DBG("Servo angle limited to %.1f deg (pulse %u ns)",
                (double)current_angle_deg, pulse_ns);
    } else {
        LOG_DBG("Servo angle %.1f deg (pulse %u ns)",
                (double)current_angle_deg, pulse_ns);
    }
    return 0;
}

/* ===== public API ===== */

int servo_init(void)
{
    if (!device_is_ready(servo_pwm.dev)) {
        LOG_ERR("Servo PWM device not ready");
        return -ENODEV;
    }

    servo_state_init(&g_state);
    return servo_set_angle(g_state.current_angle_deg);  // still uses PWM
}

int servo_apply_command(char cmd)
{
    float start_angle = g_state.current_angle_deg;

    int ret = servo_apply_command_logic(&g_state, cmd); // pure logic
    if (ret) {
        LOG_WRN("Unknown servo command '%c'", cmd);
        return ret;
    }

    int err = servo_set_angle(g_state.current_angle_deg); // do the PWM write
    if (!err) {
        LOG_INF("Servo cmd '%c': %.1f deg -> %.1f deg",
                cmd, (double)start_angle, (double)g_state.current_angle_deg);
    }
    return err;
}

/* ===== standalone UART demo (subroutine app) ===== */

static int uart_blocking_getchar(void)
{
    const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart)) {
        return -EIO;
    }

    while (1) {
        unsigned char c;
        if (uart_poll_in(uart, &c) == 0) {
            return (int)c;
        }
        k_msleep(1);
    }
}

int servo_run(void)
{
    int err = servo_init();
    if (err) {
        LOG_ERR("servo_init failed: %d", err);
        return err;
    }

    printk("\n=== Servo UART demo ===\n");
    printk("1: +90 deg, 2: -90 deg, 0/c: center\n");

    while (1) {
        int ch = uart_blocking_getchar();
        if (ch < 0) {
            LOG_ERR("UART read error: %d", ch);
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            continue;
        }

        servo_apply_command((char)ch);
    }
}
