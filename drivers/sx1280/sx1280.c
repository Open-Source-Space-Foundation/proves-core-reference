/*
 * Copyright (c) 2025 Open Source Space Foundation
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT semtech_sx1280

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/radio/sx1280.h>

LOG_MODULE_REGISTER(sx1280, CONFIG_SX1280_LOG_LEVEL);

/* Driver data structure */
struct sx1280_data {
	struct k_sem spi_sem;
	sx1280_irq_callback_t irq_callback;
	struct gpio_callback dio1_cb;
	uint16_t last_irq_status;
};

/* Driver device tree configuration structure */
struct sx1280_dt_config {
	struct spi_dt_spec spi;
	struct gpio_dt_spec reset_gpio;
	struct gpio_dt_spec busy_gpio;
	struct gpio_dt_spec dio1_gpio;
	struct gpio_dt_spec dio2_gpio;
	struct gpio_dt_spec dio3_gpio;
	struct gpio_dt_spec tx_enable_gpio;
	struct gpio_dt_spec rx_enable_gpio;
	uint16_t tcxo_delay_ms;
};

/* Helper function to wait for device not busy */
static int sx1280_wait_on_busy(const struct device *dev)
{
	const struct sx1280_dt_config *config = dev->config;
	int timeout = 1000; /* 1 second timeout */

	if (!config->busy_gpio.port) {
		/* No busy pin configured, use delay */
		k_msleep(1);
		return 0;
	}

	while (gpio_pin_get_dt(&config->busy_gpio) && timeout > 0) {
		k_usleep(100);
		timeout--;
	}

	if (timeout == 0) {
		LOG_ERR("Timeout waiting for device ready");
		return -ETIMEDOUT;
	}

	return 0;
}

/* Write a command to the SX1280 */
static int sx1280_write_command(const struct device *dev, uint8_t cmd,
				const uint8_t *data, size_t len)
{
	const struct sx1280_dt_config *config = dev->config;
	struct sx1280_data *drv_data = dev->data;
	int ret;

	k_sem_take(&drv_data->spi_sem, K_FOREVER);

	ret = sx1280_wait_on_busy(dev);
	if (ret) {
		k_sem_give(&drv_data->spi_sem);
		return ret;
	}

	uint8_t tx_buf[257]; /* Max command size */
	tx_buf[0] = cmd;
	if (data && len > 0) {
		memcpy(&tx_buf[1], data, len);
	}

	const struct spi_buf tx_spi_buf = {
		.buf = tx_buf,
		.len = len + 1,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_spi_buf,
		.count = 1,
	};

	ret = spi_write_dt(&config->spi, &tx);

	k_sem_give(&drv_data->spi_sem);
	return ret;
}

/* Read a command response from the SX1280 */
static int sx1280_read_command(const struct device *dev, uint8_t cmd,
			       uint8_t *data, size_t len)
{
	const struct sx1280_dt_config *config = dev->config;
	struct sx1280_data *drv_data = dev->data;
	int ret;

	k_sem_take(&drv_data->spi_sem, K_FOREVER);

	ret = sx1280_wait_on_busy(dev);
	if (ret) {
		k_sem_give(&drv_data->spi_sem);
		return ret;
	}

	uint8_t tx_buf[257] = {0};
	uint8_t rx_buf[257] = {0};
	tx_buf[0] = cmd;

	const struct spi_buf tx_spi_buf = {
		.buf = tx_buf,
		.len = len + 2, /* Command + status byte + data */
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_spi_buf,
		.count = 1,
	};

	const struct spi_buf rx_spi_buf = {
		.buf = rx_buf,
		.len = len + 2,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_spi_buf,
		.count = 1,
	};

	ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret == 0 && data && len > 0) {
		memcpy(data, &rx_buf[2], len); /* Skip command and status bytes */
	}

	k_sem_give(&drv_data->spi_sem);
	return ret;
}

/* Write to a register */
static int sx1280_write_register(const struct device *dev, uint16_t addr,
				 const uint8_t *data, size_t len)
{
	uint8_t buf[258];
	buf[0] = (addr >> 8) & 0xFF;
	buf[1] = addr & 0xFF;
	if (data && len > 0) {
		memcpy(&buf[2], data, len);
	}
	return sx1280_write_command(dev, SX1280_CMD_WRITE_REGISTER, buf, len + 2);
}

/* Read from a register */
static int sx1280_read_register(const struct device *dev, uint16_t addr,
				uint8_t *data, size_t len)
{
	uint8_t cmd_buf[2];
	cmd_buf[0] = (addr >> 8) & 0xFF;
	cmd_buf[1] = addr & 0xFF;

	const struct sx1280_dt_config *config = dev->config;
	struct sx1280_data *drv_data = dev->data;
	int ret;

	k_sem_take(&drv_data->spi_sem, K_FOREVER);

	ret = sx1280_wait_on_busy(dev);
	if (ret) {
		k_sem_give(&drv_data->spi_sem);
		return ret;
	}

	uint8_t tx_buf[259] = {0};
	uint8_t rx_buf[259] = {0};
	tx_buf[0] = SX1280_CMD_READ_REGISTER;
	tx_buf[1] = cmd_buf[0];
	tx_buf[2] = cmd_buf[1];

	const struct spi_buf tx_spi_buf = {
		.buf = tx_buf,
		.len = len + 4, /* Command + addr(2) + status + data */
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_spi_buf,
		.count = 1,
	};

	const struct spi_buf rx_spi_buf = {
		.buf = rx_buf,
		.len = len + 4,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_spi_buf,
		.count = 1,
	};

	ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret == 0 && data && len > 0) {
		memcpy(data, &rx_buf[4], len);
	}

	k_sem_give(&drv_data->spi_sem);
	return ret;
}

/* Hardware reset */
static int sx1280_reset(const struct device *dev)
{
	const struct sx1280_dt_config *config = dev->config;

	if (!config->reset_gpio.port) {
		return -ENODEV;
	}

	gpio_pin_set_dt(&config->reset_gpio, 0); /* Active low */
	k_msleep(20);
	gpio_pin_set_dt(&config->reset_gpio, 1);
	k_msleep(50);

	return 0;
}

/* DIO1 interrupt handler */
static void sx1280_dio1_handler(const struct device *port,
				struct gpio_callback *cb, uint32_t pins)
{
	struct sx1280_data *data = CONTAINER_OF(cb, struct sx1280_data, dio1_cb);
	const struct device *dev = DEVICE_DT_INST_GET(0);

	/* Read IRQ status */
	uint16_t irq_status;
	if (sx1280_get_irq_status(dev, &irq_status) == 0) {
		data->last_irq_status = irq_status;
		if (data->irq_callback) {
			data->irq_callback(dev, irq_status);
		}
	}
}

/* API Implementations */

int sx1280_init(const struct device *dev)
{
	const struct sx1280_dt_config *config = dev->config;
	struct sx1280_data *data = dev->data;
	int ret;

	/* Initialize semaphore */
	k_sem_init(&data->spi_sem, 1, 1);

	/* Verify SPI is ready */
	if (!spi_is_ready_dt(&config->spi)) {
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	}

	/* Configure reset GPIO */
	if (config->reset_gpio.port) {
		if (!gpio_is_ready_dt(&config->reset_gpio)) {
			LOG_ERR("Reset GPIO not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Failed to configure reset GPIO: %d", ret);
			return ret;
		}
	}

	/* Configure busy GPIO (input) */
	if (config->busy_gpio.port) {
		if (!gpio_is_ready_dt(&config->busy_gpio)) {
			LOG_ERR("Busy GPIO not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->busy_gpio, GPIO_INPUT);
		if (ret) {
			LOG_ERR("Failed to configure busy GPIO: %d", ret);
			return ret;
		}
	}

	/* Configure DIO1 GPIO (interrupt) */
	if (config->dio1_gpio.port) {
		if (!gpio_is_ready_dt(&config->dio1_gpio)) {
			LOG_ERR("DIO1 GPIO not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->dio1_gpio, GPIO_INPUT);
		if (ret) {
			LOG_ERR("Failed to configure DIO1 GPIO: %d", ret);
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&config->dio1_gpio,
						      GPIO_INT_EDGE_RISING);
		if (ret) {
			LOG_ERR("Failed to configure DIO1 interrupt: %d", ret);
			return ret;
		}

		gpio_init_callback(&data->dio1_cb, sx1280_dio1_handler,
				   BIT(config->dio1_gpio.pin));
		ret = gpio_add_callback(config->dio1_gpio.port, &data->dio1_cb);
		if (ret) {
			LOG_ERR("Failed to add DIO1 callback: %d", ret);
			return ret;
		}
	}

	/* Configure TX/RX enable GPIOs if present */
	if (config->tx_enable_gpio.port) {
		ret = gpio_pin_configure_dt(&config->tx_enable_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Failed to configure TX enable GPIO: %d", ret);
			return ret;
		}
	}

	if (config->rx_enable_gpio.port) {
		ret = gpio_pin_configure_dt(&config->rx_enable_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Failed to configure RX enable GPIO: %d", ret);
			return ret;
		}
	}

	/* Reset the radio */
	ret = sx1280_reset(dev);
	if (ret) {
		LOG_ERR("Failed to reset radio: %d", ret);
		return ret;
	}

	/* Set to standby mode */
	uint8_t standby_cmd = SX1280_STANDBY_RC;
	ret = sx1280_write_command(dev, SX1280_CMD_SET_STANDBY, &standby_cmd, 1);
	if (ret) {
		LOG_ERR("Failed to set standby mode: %d", ret);
		return ret;
	}

	/* Set packet type to LoRa */
	uint8_t packet_type = SX1280_PACKET_TYPE_LORA;
	ret = sx1280_write_command(dev, 0x8A, &packet_type, 1); /* SET_PACKET_TYPE */
	if (ret) {
		LOG_ERR("Failed to set packet type: %d", ret);
		return ret;
	}

	LOG_INF("SX1280 initialized successfully");
	return 0;
}

int sx1280_configure(const struct device *dev, const struct sx1280_config *config)
{
	int ret;

	/* Set RF frequency (2.4GHz band) */
	uint32_t freq_reg = (uint32_t)((double)config->frequency_hz / (double)198.3642578125);
	uint8_t freq_buf[3];
	freq_buf[0] = (freq_reg >> 16) & 0xFF;
	freq_buf[1] = (freq_reg >> 8) & 0xFF;
	freq_buf[2] = freq_reg & 0xFF;
	ret = sx1280_write_command(dev, SX1280_CMD_SET_RF_FREQUENCY, freq_buf, 3);
	if (ret) {
		return ret;
	}

	/* Set modulation parameters (SF, BW, CR) */
	uint8_t mod_params[3];
	mod_params[0] = config->spreading_factor;
	mod_params[1] = config->bandwidth;
	mod_params[2] = config->coding_rate;
	ret = sx1280_write_command(dev, SX1280_CMD_SET_MODULATION_PARAMS, mod_params, 3);
	if (ret) {
		return ret;
	}

	/* Set packet parameters */
	uint8_t pkt_params[7];
	pkt_params[0] = config->preamble_length;
	pkt_params[1] = config->implicit_header ? SX1280_LORA_PACKET_IMPLICIT : SX1280_LORA_PACKET_EXPLICIT;
	pkt_params[2] = config->payload_length;
	pkt_params[3] = config->crc_on ? SX1280_LORA_CRC_ON : SX1280_LORA_CRC_OFF;
	pkt_params[4] = SX1280_LORA_IQ_NORMAL;
	pkt_params[5] = 0x00;
	pkt_params[6] = 0x00;
	ret = sx1280_write_command(dev, SX1280_CMD_SET_PACKET_PARAMS, pkt_params, 7);
	if (ret) {
		return ret;
	}

	/* Set TX parameters */
	uint8_t tx_params[2];
	tx_params[0] = config->tx_power_dbm;
	tx_params[1] = 0xE0; /* Ramp time 20us */
	ret = sx1280_write_command(dev, SX1280_CMD_SET_TX_PARAMS, tx_params, 2);
	if (ret) {
		return ret;
	}

	/* Configure DIO and IRQ */
	uint8_t irq_params[8];
	irq_params[0] = (SX1280_IRQ_ALL >> 8) & 0xFF;
	irq_params[1] = SX1280_IRQ_ALL & 0xFF;
	irq_params[2] = (SX1280_IRQ_ALL >> 8) & 0xFF;
	irq_params[3] = SX1280_IRQ_ALL & 0xFF;
	irq_params[4] = 0x00;
	irq_params[5] = 0x00;
	irq_params[6] = 0x00;
	irq_params[7] = 0x00;
	ret = sx1280_write_command(dev, SX1280_CMD_SET_DIO_IRQ_PARAMS, irq_params, 8);

	return ret;
}

int sx1280_set_standby(const struct device *dev)
{
	uint8_t standby_cmd = SX1280_STANDBY_RC;
	return sx1280_write_command(dev, SX1280_CMD_SET_STANDBY, &standby_cmd, 1);
}

int sx1280_set_tx(const struct device *dev, uint16_t timeout_ms)
{
	const struct sx1280_dt_config *config = dev->config;

	/* Enable TX path */
	if (config->tx_enable_gpio.port) {
		gpio_pin_set_dt(&config->tx_enable_gpio, 1);
	}
	if (config->rx_enable_gpio.port) {
		gpio_pin_set_dt(&config->rx_enable_gpio, 0);
	}

	/* Calculate timeout value (in units of 15.625us) */
	uint16_t timeout_val = (timeout_ms * 64);

	uint8_t tx_params[3];
	tx_params[0] = 0x00; /* Periodbase = 15.625us */
	tx_params[1] = (timeout_val >> 8) & 0xFF;
	tx_params[2] = timeout_val & 0xFF;
	return sx1280_write_command(dev, SX1280_CMD_SET_TX, tx_params, 3);
}

int sx1280_set_rx(const struct device *dev, uint16_t timeout_ms)
{
	const struct sx1280_dt_config *config = dev->config;

	/* Enable RX path */
	if (config->rx_enable_gpio.port) {
		gpio_pin_set_dt(&config->rx_enable_gpio, 1);
	}
	if (config->tx_enable_gpio.port) {
		gpio_pin_set_dt(&config->tx_enable_gpio, 0);
	}

	/* Calculate timeout value */
	uint16_t timeout_val = (timeout_ms == 0xFFFF) ? 0xFFFF : (timeout_ms * 64);

	uint8_t rx_params[3];
	rx_params[0] = 0x00; /* Periodbase = 15.625us */
	rx_params[1] = (timeout_val >> 8) & 0xFF;
	rx_params[2] = timeout_val & 0xFF;
	return sx1280_write_command(dev, SX1280_CMD_SET_RX, rx_params, 3);
}

int sx1280_write_buffer(const struct device *dev, const uint8_t *data, size_t len)
{
	uint8_t buf[257];
	buf[0] = 0x00; /* Offset */
	if (data && len > 0 && len <= 255) {
		memcpy(&buf[1], data, len);
		return sx1280_write_command(dev, SX1280_CMD_WRITE_BUFFER, buf, len + 1);
	}
	return -EINVAL;
}

int sx1280_read_buffer(const struct device *dev, uint8_t *data, size_t len,
		       size_t *bytes_read)
{
	int ret;
	uint8_t rx_status[2];

	/* Get RX buffer status */
	ret = sx1280_read_command(dev, SX1280_CMD_GET_RX_BUFFER_STATUS, rx_status, 2);
	if (ret) {
		return ret;
	}

	uint8_t payload_len = rx_status[0];
	uint8_t offset = rx_status[1];

	if (payload_len > len) {
		payload_len = len;
	}

	if (payload_len == 0) {
		*bytes_read = 0;
		return 0;
	}

	/* Read buffer */
	const struct sx1280_dt_config *config = dev->config;
	struct sx1280_data *drv_data = dev->data;

	k_sem_take(&drv_data->spi_sem, K_FOREVER);

	ret = sx1280_wait_on_busy(dev);
	if (ret) {
		k_sem_give(&drv_data->spi_sem);
		return ret;
	}

	uint8_t tx_buf[258] = {0};
	uint8_t rx_buf[258] = {0};
	tx_buf[0] = SX1280_CMD_READ_BUFFER;
	tx_buf[1] = offset;

	const struct spi_buf tx_spi_buf = {
		.buf = tx_buf,
		.len = payload_len + 3, /* Command + offset + status + data */
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_spi_buf,
		.count = 1,
	};

	const struct spi_buf rx_spi_buf = {
		.buf = rx_buf,
		.len = payload_len + 3,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_spi_buf,
		.count = 1,
	};

	ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret == 0 && data) {
		memcpy(data, &rx_buf[3], payload_len);
		*bytes_read = payload_len;
	}

	k_sem_give(&drv_data->spi_sem);
	return ret;
}

int sx1280_get_irq_status(const struct device *dev, uint16_t *irq_status)
{
	uint8_t status[2];
	int ret = sx1280_read_command(dev, SX1280_CMD_GET_IRQ_STATUS, status, 2);
	if (ret == 0 && irq_status) {
		*irq_status = ((uint16_t)status[0] << 8) | status[1];
	}
	return ret;
}

int sx1280_clear_irq_status(const struct device *dev, uint16_t irq_mask)
{
	uint8_t buf[2];
	buf[0] = (irq_mask >> 8) & 0xFF;
	buf[1] = irq_mask & 0xFF;
	return sx1280_write_command(dev, SX1280_CMD_CLR_IRQ_STATUS, buf, 2);
}

int sx1280_get_packet_status(const struct device *dev,
			      struct sx1280_packet_status *status)
{
	uint8_t pkt_status[5];
	int ret = sx1280_read_command(dev, SX1280_CMD_GET_PACKET_STATUS, pkt_status, 5);
	if (ret == 0 && status) {
		status->rssi = -(int16_t)pkt_status[0] / 2;
		status->snr = (int8_t)pkt_status[1] / 4;
		status->sync_error = pkt_status[2];
	}
	return ret;
}

int sx1280_register_irq_callback(const struct device *dev,
				  sx1280_irq_callback_t callback)
{
	struct sx1280_data *data = dev->data;
	data->irq_callback = callback;
	return 0;
}

int sx1280_get_packet_length(const struct device *dev, uint8_t *length)
{
	uint8_t rx_status[2];
	int ret = sx1280_read_command(dev, SX1280_CMD_GET_RX_BUFFER_STATUS, rx_status, 2);
	if (ret == 0 && length) {
		*length = rx_status[0];
	}
	return ret;
}

/* Driver initialization */
static int sx1280_driver_init(const struct device *dev)
{
	return sx1280_init(dev);
}

/* Device instantiation macro */
#define SX1280_DEFINE(inst)							\
	static struct sx1280_data sx1280_data_##inst;				\
										\
	static const struct sx1280_dt_config sx1280_config_##inst = {		\
		.spi = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |		\
					    SPI_WORD_SET(8) |			\
					    SPI_TRANSFER_MSB, 0),		\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),\
		.busy_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, busy_gpios, {0}),	\
		.dio1_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, dio1_gpios, {0}),	\
		.dio2_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, dio2_gpios, {0}),	\
		.dio3_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, dio3_gpios, {0}),	\
		.tx_enable_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, tx_enable_gpios, {0}), \
		.rx_enable_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, rx_enable_gpios, {0}), \
		.tcxo_delay_ms = DT_INST_PROP_OR(inst, tcxo_power_startup_delay_ms, 10), \
	};									\
										\
	DEVICE_DT_INST_DEFINE(inst, sx1280_driver_init, NULL,			\
			      &sx1280_data_##inst, &sx1280_config_##inst,	\
			      POST_KERNEL, CONFIG_SX1280_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(SX1280_DEFINE)
