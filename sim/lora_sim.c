/*
 * Null LoRa radio for native_sim. fprime-zephyr's LoRa component asserts that
 * the radio is ready, so the board needs one. It implements the four calls that
 * component makes. Sends are dropped and nothing is ever received.
 */
#define DT_DRV_COMPAT proves_lora_sim

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>

static int lora_sim_config(const struct device* dev, struct lora_modem_config* config) {
    ARG_UNUSED(dev);
    ARG_UNUSED(config);
    return 0;
}

static int lora_sim_send(const struct device* dev, uint8_t* data, uint32_t data_len) {
    ARG_UNUSED(dev);
    ARG_UNUSED(data);
    ARG_UNUSED(data_len);
    return 0;
}

static int lora_sim_recv_async(const struct device* dev, lora_recv_cb cb, void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(user_data);
    return 0;
}

static int lora_sim_test_cw(const struct device* dev, uint32_t frequency, int8_t tx_power, uint16_t duration) {
    ARG_UNUSED(dev);
    ARG_UNUSED(frequency);
    ARG_UNUSED(tx_power);
    ARG_UNUSED(duration);
    return 0;
}

static DEVICE_API(lora, lora_sim_api) = {
    .config = lora_sim_config,
    .send = lora_sim_send,
    .recv_async = lora_sim_recv_async,
    .test_cw = lora_sim_test_cw,
};

#define LORA_SIM_DEFINE(inst) \
    DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &lora_sim_api);

DT_INST_FOREACH_STATUS_OKAY(LORA_SIM_DEFINE)
