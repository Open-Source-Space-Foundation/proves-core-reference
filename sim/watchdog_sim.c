/*
 * Fake external watchdog. On the flight board the Watchdog component toggles a
 * GPIO every second, and an external watchdog power-cycles the board when the
 * toggling stops (with the JP6 jumper bridged). This polls the same pin on
 * gpio-emul and cold-reboots the sim after timeout-ms without a toggle.
 */
#define DT_DRV_COMPAT proves_watchdog_sim

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(watchdog_sim, LOG_LEVEL_INF);

#define WATCHDOG_SIM_POLL_MS 100

struct watchdog_sim_config {
    struct gpio_dt_spec wdi;
    uint32_t timeout_ms;
};

struct watchdog_sim_data {
    const struct device* dev;
    struct k_work_delayable poll;
    int last_level;
    int64_t last_toggle_ms;
};

static void watchdog_sim_poll(struct k_work* work) {
    struct k_work_delayable* dwork = k_work_delayable_from_work(work);
    struct watchdog_sim_data* data = CONTAINER_OF(dwork, struct watchdog_sim_data, poll);
    const struct watchdog_sim_config* cfg = data->dev->config;
    int level = gpio_emul_output_get(cfg->wdi.port, cfg->wdi.pin);
    int64_t now = k_uptime_get();

    if (level != data->last_level) {
        data->last_level = level;
        data->last_toggle_ms = now;
    } else if (now - data->last_toggle_ms >= cfg->timeout_ms) {
        LOG_WRN("no watchdog toggle for %u ms, power-cycling", cfg->timeout_ms);
        sys_reboot(SYS_REBOOT_COLD);
    }
    k_work_schedule(dwork, K_MSEC(WATCHDOG_SIM_POLL_MS));
}

static int watchdog_sim_init(const struct device* dev) {
    struct watchdog_sim_data* data = dev->data;

    data->dev = dev;
    data->last_level = -1;
    data->last_toggle_ms = k_uptime_get();
    k_work_init_delayable(&data->poll, watchdog_sim_poll);
    k_work_schedule(&data->poll, K_MSEC(WATCHDOG_SIM_POLL_MS));
    return 0;
}

#define WATCHDOG_SIM_INIT(n)                                                                                         \
    static const struct watchdog_sim_config watchdog_sim_config_##n = {                                              \
        .wdi = GPIO_DT_SPEC_INST_GET(n, wdi_gpios),                                                                  \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                                   \
    };                                                                                                               \
    static struct watchdog_sim_data watchdog_sim_data_##n;                                                           \
    DEVICE_DT_INST_DEFINE(n, watchdog_sim_init, NULL, &watchdog_sim_data_##n, &watchdog_sim_config_##n, POST_KERNEL, \
                          CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(WATCHDOG_SIM_INIT)
