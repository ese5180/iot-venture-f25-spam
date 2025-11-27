#include "servo.h"
#include "servo_logic.h"

#include <stdbool.h>
#include <errno.h>
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
             "SERVO_NODE device tree node is not enabled");

static const struct pwm_dt_spec servo_pwm = PWM_DT_SPEC_GET(SERVO_NODE);

/* Sail winch servo pulse window (nanoseconds) from devicetree.
 * pwm-servo binding usually provides these as min-pulse / max-pulse.
 */
static const uint32_t servo_min_pulse_ns = DT_PROP(SERVO_NODE, min_pulse);
static const uint32_t servo_max_pulse_ns = DT_PROP(SERVO_NODE, max_pulse);

/* Use the chosen console UART for command input */
static const struct device *const uart_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* ===== Logical state ===== */

static struct servo_state g_state;
static float current_angle_deg = 0.0f;

/* ===== Helper functions ===== */

static inline uint32_t center_ns(void)
{
    return (servo_min_pulse_ns + servo_max_pulse_ns) / 2U;
}

/* From your old tested HS-785HB code:
 * ~1890 degrees of mechanical travel across the full pulse window.
 * That gives ns-per-degree derived from the DT-configured pulse range.
 */
static inline float ns_per_deg(void)
{
    return ((float)(servo_max_pulse_ns - servo_min_pulse_ns)) / 1890.0f;
}

/* Map a logical angle (deg from center) to a PWM pulse in ns. */
static uint32_t angle_to_pulse_ns(float angle_deg, bool *limited_out)
{
    bool limited = false;

    /* Clamp using the same logic you z-test (servo_logic.h) */
    float target = servo_clamp_angle(angle_deg);
    if (target != angle_deg) {
        limited = true;
    }

    const uint32_t cen = center_ns();
    const float per_deg = ns_per_deg();

    float pulse_f = (float)cen + target * per_deg;

    if (pulse_f < (float)servo_min_pulse_ns) {
        pulse_f = (float)servo_min_pulse_ns;
        limited = true;
    } else if (pulse_f > (float)servo_max_pulse_ns) {
        pulse_f = (float)servo_max_pulse_ns;
        limited = true;
    }

    if (limited_out) {
        *limited_out = limited;
    }

    if (pulse_f < 0.0f) {
        pulse_f = 0.0f;
    }

    /* Round to nearest ns */
    return (uint32_t)(pulse_f + 0.5f);
}

/* Set the servo to a logical angle (degrees from center). */
static int servo_set_angle(float angle_deg)
{
    if (!device_is_ready(servo_pwm.dev)) {
        LOG_ERR("Servo PWM device not ready");
        return -ENODEV;
    }

    bool limited = false;
    uint32_t pulse_ns = angle_to_pulse_ns(angle_deg, &limited);

    int err = pwm_set_pulse_dt(&servo_pwm, pulse_ns);
    if (err) {
        LOG_ERR("Failed to set PWM pulse: %d", err);
        return err;
    }

    /* Keep logical state in sync with what we actually commanded */
    float clamped = servo_clamp_angle(angle_deg);
    current_angle_deg = clamped;
    g_state.current_angle_deg = clamped;

    LOG_INF("Servo angle %.1f deg (pulse %u ns)%s",
            (double)current_angle_deg,
            pulse_ns,
            limited ? " [limited]" : "");

    return 0;
}

/* Simple blocking getchar using the console UART */
static int uart_blocking_getchar(void)
{
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    unsigned char c;
    int ret;

    while (true) {
        ret = uart_poll_in(uart_dev, &c);
        if (ret == 0) {
            return (int)c;
        }
        if (ret < 0) {
            /* Real UART error */
            return ret;
        }
        /* No data yet; yield a bit */
        k_sleep(K_MSEC(10));
    }
}

/* ===== Public API ===== */

int servo_init(void)
{
    int err;

    if (!device_is_ready(servo_pwm.dev)) {
        LOG_ERR("Servo PWM device not ready");
        return -ENODEV;
    }

    if (!device_is_ready(uart_dev)) {
        LOG_WRN("UART console device not ready; commands may not work");
    }

    servo_state_init(&g_state);

    /* Center the servo at startup */
    err = servo_set_angle(0.0f);
    if (err) {
        return err;
    }

    LOG_INF("Servo initialized: min=%u ns max=%u ns center=%u ns",
            servo_min_pulse_ns,
            servo_max_pulse_ns,
            center_ns());

    return 0;
}

int servo_apply_command(char cmd)
{
    float start_angle = current_angle_deg;

    /* Update logical state using pure logic (ztest-covered) */
    int ret = servo_apply_command_logic(&g_state, cmd);
    if (ret) {
        LOG_WRN("Unknown servo command '%c' (ret=%d)", cmd, ret);
        return ret;
    }

    /* Drive hardware to new logical angle */
    int err = servo_set_angle(g_state.current_angle_deg);
    if (err) {
        LOG_ERR("servo_set_angle failed: %d", err);
        return err;
    }

    LOG_INF("Command '%c': %.1f -> %.1f deg",
            cmd,
            (double)start_angle,
            (double)g_state.current_angle_deg);

    return 0;
}

/* Demo-style entry point: init + UART command loop */
int servo_run(void)
{
    int err = servo_init();
    if (err) {
        LOG_ERR("servo_init failed: %d", err);
        return err;
    }

    printk("Sail winch servo control ready.\n");
    printk("Commands: '1'=+90 deg, '2'=-90 deg, '0'/'c'/'C'=center.\n");

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
