/*
 * GPIO controller whose pins read back their output latch, as the flight
 * board's MCP23017 does. LoadSwitch configures its pin as output only and
 * reads it back, and gpio-emul returns 0 for such a pin.
 */
#define DT_DRV_COMPAT proves_gpio_latch_sim

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>

struct gpio_latch_sim_config {
    struct gpio_driver_config common;
};

struct gpio_latch_sim_data {
    struct gpio_driver_data common;
    atomic_t latch;
};

static int gpio_latch_sim_pin_configure(const struct device* dev, gpio_pin_t pin, gpio_flags_t flags) {
    struct gpio_latch_sim_data* data = dev->data;

    if (flags & GPIO_OUTPUT_INIT_HIGH) {
        atomic_or(&data->latch, BIT(pin));
    } else if (flags & GPIO_OUTPUT_INIT_LOW) {
        atomic_and(&data->latch, ~BIT(pin));
    }
    return 0;
}

static int gpio_latch_sim_port_get_raw(const struct device* dev, gpio_port_value_t* value) {
    struct gpio_latch_sim_data* data = dev->data;

    *value = (gpio_port_value_t)atomic_get(&data->latch);
    return 0;
}

static int gpio_latch_sim_port_set_masked_raw(const struct device* dev,
                                              gpio_port_pins_t mask,
                                              gpio_port_value_t value) {
    struct gpio_latch_sim_data* data = dev->data;
    atomic_val_t old;

    do {
        old = atomic_get(&data->latch);
    } while (!atomic_cas(&data->latch, old, (old & ~mask) | (value & mask)));
    return 0;
}

static int gpio_latch_sim_port_set_bits_raw(const struct device* dev, gpio_port_pins_t pins) {
    struct gpio_latch_sim_data* data = dev->data;

    atomic_or(&data->latch, pins);
    return 0;
}

static int gpio_latch_sim_port_clear_bits_raw(const struct device* dev, gpio_port_pins_t pins) {
    struct gpio_latch_sim_data* data = dev->data;

    atomic_and(&data->latch, ~pins);
    return 0;
}

static int gpio_latch_sim_port_toggle_bits(const struct device* dev, gpio_port_pins_t pins) {
    struct gpio_latch_sim_data* data = dev->data;

    atomic_xor(&data->latch, pins);
    return 0;
}

static DEVICE_API(gpio, gpio_latch_sim_api) = {
    .pin_configure = gpio_latch_sim_pin_configure,
    .port_get_raw = gpio_latch_sim_port_get_raw,
    .port_set_masked_raw = gpio_latch_sim_port_set_masked_raw,
    .port_set_bits_raw = gpio_latch_sim_port_set_bits_raw,
    .port_clear_bits_raw = gpio_latch_sim_port_clear_bits_raw,
    .port_toggle_bits = gpio_latch_sim_port_toggle_bits,
};

#define GPIO_LATCH_SIM_INIT(n)                                                                              \
    static const struct gpio_latch_sim_config gpio_latch_sim_config_##n = {                                 \
        .common = {.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n)},                                    \
    };                                                                                                      \
    static struct gpio_latch_sim_data gpio_latch_sim_data_##n;                                              \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &gpio_latch_sim_data_##n, &gpio_latch_sim_config_##n, POST_KERNEL, \
                          CONFIG_GPIO_INIT_PRIORITY, &gpio_latch_sim_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_LATCH_SIM_INIT)
