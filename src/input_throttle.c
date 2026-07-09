// SPDX-License-Identifier: GPL-2.0-or-later

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT torabo_input_throttle

LOG_MODULE_REGISTER(torabo_input_throttle, CONFIG_ZMK_LOG_LEVEL);

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

struct input_throttle_config {
    uint32_t interval_ms;
};

struct input_throttle_data {
    const struct device *dev;
    struct k_work_delayable work;
    atomic_t pending[ARRAY_SIZE(throttle_codes)];
    atomic_t dropped_reports;
};

static void log_report_drop(struct input_throttle_data *data, int err) {
    int drops = atomic_inc(&data->dropped_reports) + 1;

    if (drops == 1 || (drops % 32) == 0) {
        LOG_WRN("%s dropped input report: err=%d drops=%d", data->dev->name, err, drops);
    }
}

static void throttle_work_handler(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct input_throttle_data *data = CONTAINER_OF(delayable, struct input_throttle_data, work);
    const struct input_throttle_config *config = data->dev->config;
    int values[ARRAY_SIZE(throttle_codes)];
    int last = -1;

    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        values[i] = atomic_set(&data->pending[i], 0);
        if (values[i] != 0) {
            last = i;
        }
    }

    /* Emit every non-zero axis; only the final one carries the sync flag. */
    for (int i = 0; i <= last; i++) {
        if (values[i] != 0) {
            int err = input_report_rel(data->dev, throttle_codes[i], values[i], i == last, K_NO_WAIT);

            if (err < 0) {
                log_report_drop(data, err);
            }
        }
    }

    /* Reschedule if new deltas arrived while we were reporting. */
    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        if (atomic_get(&data->pending[i]) != 0) {
            k_work_schedule(&data->work, K_MSEC(config->interval_ms));
            break;
        }
    }
}

static void input_throttle_handle_event(struct input_throttle_data *data, struct input_event *evt) {
    const struct input_throttle_config *config = data->dev->config;

    if (evt->type != INPUT_EV_REL) {
        return;
    }

    for (int i = 0; i < (int)ARRAY_SIZE(throttle_codes); i++) {
        if (evt->code == throttle_codes[i]) {
            atomic_add(&data->pending[i], evt->value);
            k_work_schedule(&data->work, K_MSEC(config->interval_ms));
            return;
        }
    }
}

static int input_throttle_init(const struct device *dev) {
    struct input_throttle_data *data = dev->data;

    data->dev = dev;
    k_work_init_delayable(&data->work, throttle_work_handler);
    return 0;
}

#define INPUT_THROTTLE_DEFINE(inst)                                                                 \
    static struct input_throttle_data input_throttle_data_##inst;                                    \
    static const struct input_throttle_config input_throttle_config_##inst = {                       \
        .interval_ms = DT_INST_PROP(inst, throttle_interval_ms),                                     \
    };                                                                                              \
                                                                                                     \
    static void input_throttle_callback_##inst(struct input_event *evt) {                            \
        input_throttle_handle_event(&input_throttle_data_##inst, evt);                               \
    }                                                                                                \
                                                                                                     \
    INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(inst, source)), input_throttle_callback_##inst); \
                                                                                                     \
    DEVICE_DT_INST_DEFINE(inst, input_throttle_init, NULL, &input_throttle_data_##inst,              \
                          &input_throttle_config_##inst, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(INPUT_THROTTLE_DEFINE)
