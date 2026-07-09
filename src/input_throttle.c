// SPDX-License-Identifier: GPL-2.0-or-later

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define THROTTLE_INTERVAL_MS 30

static const struct device *const throttle_dev = DEVICE_DT_GET(DT_NODELABEL(pointing_device_throttled));

/*
 * Relative axes we aggregate and re-emit. The trackball reports REL_X/REL_Y;
 * the trackpad in scroller-mode reports REL_WHEEL/REL_HWHEEL. Handling all of
 * them keeps this proxy usable for either source device unchanged.
 */
static const uint16_t throttle_codes[] = {
    INPUT_REL_X,
    INPUT_REL_Y,
    INPUT_REL_WHEEL,
    INPUT_REL_HWHEEL,
};
static atomic_t pending[ARRAY_SIZE(throttle_codes)];
static struct k_work_delayable throttle_work;

static void throttle_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    int values[ARRAY_SIZE(throttle_codes)];
    int last = -1;

    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        values[i] = atomic_set(&pending[i], 0);
        if (values[i] != 0) {
            last = i;
        }
    }

    /* Emit every non-zero axis; only the final one carries the sync flag. */
    for (int i = 0; i <= last; i++) {
        if (values[i] != 0) {
            input_report_rel(throttle_dev, throttle_codes[i], values[i], i == last, K_NO_WAIT);
        }
    }

    /* Reschedule if new deltas arrived while we were reporting. */
    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        if (atomic_get(&pending[i]) != 0) {
            k_work_schedule(&throttle_work, K_MSEC(THROTTLE_INTERVAL_MS));
            break;
        }
    }
}

static void pointing_input_callback(struct input_event *evt) {
    if (evt->type != INPUT_EV_REL) {
        return;
    }

    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        if (evt->code == throttle_codes[i]) {
            atomic_add(&pending[i], evt->value);
            k_work_schedule(&throttle_work, K_MSEC(THROTTLE_INTERVAL_MS));
            return;
        }
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pointing_device)), pointing_input_callback);

static int input_throttle_init(const struct device *dev) {
    ARG_UNUSED(dev);

    k_work_init_delayable(&throttle_work, throttle_work_handler);
    return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(pointing_device_throttled), input_throttle_init, NULL, NULL, NULL,
                 POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);
