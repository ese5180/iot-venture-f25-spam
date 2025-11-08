#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(servo_step, LOG_LEVEL_INF);

/* Devicetree: node must be labeled 'servo' in your overlay.
 * Expected properties on that node:
 *   pwms = <&pwm0 ch PERIOD POLARITY>;   // e.g., 20 ms (50 Hz)
 *   min-pulse = <PWM_USEC(900)>;
 *   max-pulse = <PWM_USEC(2100)>;
 */
#define SERVO_NODE DT_NODELABEL(servo)
BUILD_ASSERT(DT_NODE_HAS_STATUS(SERVO_NODE, okay), "DT node 'servo' not found/enabled");

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(SERVO_NODE);
static const uint32_t min_pulse_ns = DT_PROP(SERVO_NODE, min_pulse);
static const uint32_t max_pulse_ns = DT_PROP(SERVO_NODE, max_pulse);

static inline uint32_t ns_to_us(uint32_t ns) { return ns / 1000U; }

static float current_deg = 0.0f;

static inline uint32_t center_ns(void)
{
    return (min_pulse_ns + max_pulse_ns) / 2U;
}

static inline float ns_per_deg(void)
{
    /* HS-785HB ≈ 1890° across the 900–2100 µs window */
    return ((float)(max_pulse_ns - min_pulse_ns)) / 1890.0f;
}

/* Command a relative angle (deg from center), clamped to [min,max] */
static int servo_write_deg(float deg)
{
    const uint32_t cen = center_ns();
    const float per_deg = ns_per_deg();
    float target_ns_f = (float)cen + deg * per_deg;

    if (target_ns_f < (float)min_pulse_ns) target_ns_f = (float)min_pulse_ns;
    if (target_ns_f > (float)max_pulse_ns) target_ns_f = (float)max_pulse_ns;

    const uint32_t target_ns = (uint32_t)(target_ns_f + 0.5f);
    int rc = pwm_set_pulse_dt(&servo, target_ns);
    if (!rc) {
        LOG_INF("Pulse: %u us (deg_from_center=%.1f)",
                ns_to_us(target_ns), (double)deg);   // cast silences -Wdouble-promotion
    }
    return rc;
}

/* Blocking getchar via UART poll on the chosen console UART */
static int uart_blocking_getchar(void)
{
    const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart)) {
        return -EIO;
    }
    for (;;) {
        unsigned char c;
        if (uart_poll_in(uart, &c) == 0) {
            return (int)c;
        }
        k_msleep(1);  // small wait to avoid busy spin
    }
}

void main(void)
{
    if (!pwm_is_ready_dt(&servo)) {
        LOG_ERR("PWM device not ready");
        return;
    }

    const uint32_t cen = center_ns();
    const float per_deg = ns_per_deg();
    const float half_range_deg = ((float)(max_pulse_ns - min_pulse_ns) / 2.0f) / per_deg;

    LOG_INF("HS-785HB UART step demo");
    LOG_INF("Period: %u us, min:%u us, center:%u us, max:%u us",
            ns_to_us(servo.period), ns_to_us(min_pulse_ns),
            ns_to_us(cen), ns_to_us(max_pulse_ns));
    LOG_INF("Mapping: %.3f ns/deg  (~%.3f us/90deg), range: +/- %.1f deg",
            (double)per_deg, (double)((per_deg * 90.0f) / 1000.0f), (double)half_range_deg);

    /* Start at center */
    (void)pwm_set_pulse_dt(&servo, cen);
    current_deg = 0.0f;
    k_msleep(300);

    printk("\n=== Sail Winch Control ===\n");
    printk("Press '1' + Enter: +90 deg\n");
    printk("Press '2' + Enter: -90 deg\n");
    printk("Press 'c' + Enter: center\n\n");

    while (1) {
        int ch = uart_blocking_getchar();
        if (ch < 0) {
            LOG_ERR("UART read error: %d", ch);
            continue;
        }

        if (ch == '1') {
            float next = current_deg + 90.0f;
            if (next >  half_range_deg) next =  half_range_deg;
            if (servo_write_deg(next) == 0) current_deg = next;
        } else if (ch == '2') {
            float next = current_deg - 90.0f;
            if (next < -half_range_deg) next = -half_range_deg;
            if (servo_write_deg(next) == 0) current_deg = next;
        } else if (ch == 'c' || ch == 'C') {
            if (servo_write_deg(0.0f) == 0) current_deg = 0.0f;
        } else if (ch == '\r' || ch == '\n') {
            /* ignore */
        } else {
            printk("Unknown key '%c'. Use 1 (+90), 2 (-90), c (center)\n", ch);
        }
    }
}
