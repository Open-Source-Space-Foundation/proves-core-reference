/*
 * Copyright (c) 2025 Open Source Space Foundation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_RADIO_SX1280_H_
#define ZEPHYR_INCLUDE_DRIVERS_RADIO_SX1280_H_

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SX1280 Radio Driver API
 * @defgroup sx1280_interface SX1280 Radio Driver API
 * @ingroup io_interfaces
 * @{
 */

/* SX1280 Radio Commands */
#define SX1280_CMD_GET_STATUS           0xC0
#define SX1280_CMD_WRITE_REGISTER       0x18
#define SX1280_CMD_READ_REGISTER        0x19
#define SX1280_CMD_WRITE_BUFFER         0x1A
#define SX1280_CMD_READ_BUFFER          0x1B
#define SX1280_CMD_SET_SLEEP            0x84
#define SX1280_CMD_SET_STANDBY          0x80
#define SX1280_CMD_SET_FS               0xC1
#define SX1280_CMD_SET_TX               0x83
#define SX1280_CMD_SET_RX               0x82
#define SX1280_CMD_SET_RF_FREQUENCY     0x86
#define SX1280_CMD_SET_TX_PARAMS        0x8E
#define SX1280_CMD_SET_MODULATION_PARAMS 0x8B
#define SX1280_CMD_SET_PACKET_PARAMS    0x8C
#define SX1280_CMD_GET_RX_BUFFER_STATUS 0x17
#define SX1280_CMD_GET_PACKET_STATUS    0x1D
#define SX1280_CMD_GET_RSSI_INST        0x1F
#define SX1280_CMD_SET_DIO_IRQ_PARAMS   0x8D
#define SX1280_CMD_GET_IRQ_STATUS       0x15
#define SX1280_CMD_CLR_IRQ_STATUS       0x97
#define SX1280_CMD_SET_AUTO_TX          0x98

/* SX1280 Registers */
#define SX1280_REG_LR_FIRMWARE_VERSION_MSB  0x0153

/* Packet Types */
#define SX1280_PACKET_TYPE_GFSK         0x00
#define SX1280_PACKET_TYPE_LORA         0x01
#define SX1280_PACKET_TYPE_RANGING      0x02
#define SX1280_PACKET_TYPE_FLRC         0x03
#define SX1280_PACKET_TYPE_BLE          0x04

/* Standby Modes */
#define SX1280_STANDBY_RC               0x00
#define SX1280_STANDBY_XOSC             0x01

/* IRQ Masks */
#define SX1280_IRQ_TX_DONE              0x0001
#define SX1280_IRQ_RX_DONE              0x0002
#define SX1280_IRQ_SYNC_WORD_VALID      0x0004
#define SX1280_IRQ_SYNC_WORD_ERROR      0x0008
#define SX1280_IRQ_HEADER_VALID         0x0010
#define SX1280_IRQ_HEADER_ERROR         0x0020
#define SX1280_IRQ_CRC_ERROR            0x0040
#define SX1280_IRQ_RANGING_SLAVE_RESPONSE_DONE 0x0080
#define SX1280_IRQ_RANGING_SLAVE_REQUEST_DISCARDED 0x0100
#define SX1280_IRQ_RANGING_MASTER_RESULT_VALID 0x0200
#define SX1280_IRQ_RANGING_MASTER_TIMEOUT 0x0400
#define SX1280_IRQ_RANGING_SLAVE_REQUEST_VALID 0x0800
#define SX1280_IRQ_CAD_DONE             0x1000
#define SX1280_IRQ_CAD_DETECTED         0x2000
#define SX1280_IRQ_RX_TX_TIMEOUT        0x4000
#define SX1280_IRQ_PREAMBLE_DETECTED    0x8000
#define SX1280_IRQ_ALL                  0xFFFF

/* LoRa Spreading Factors */
#define SX1280_LORA_SF5                 0x50
#define SX1280_LORA_SF6                 0x60
#define SX1280_LORA_SF7                 0x70
#define SX1280_LORA_SF8                 0x80
#define SX1280_LORA_SF9                 0x90
#define SX1280_LORA_SF10                0xA0
#define SX1280_LORA_SF11                0xB0
#define SX1280_LORA_SF12                0xC0

/* LoRa Bandwidths */
#define SX1280_LORA_BW_203              0x34
#define SX1280_LORA_BW_406              0x26
#define SX1280_LORA_BW_812              0x18
#define SX1280_LORA_BW_1625             0x0A

/* LoRa Coding Rates */
#define SX1280_LORA_CR_4_5              0x01
#define SX1280_LORA_CR_4_6              0x02
#define SX1280_LORA_CR_4_7              0x03
#define SX1280_LORA_CR_4_8              0x04
#define SX1280_LORA_CR_LI_4_5           0x05
#define SX1280_LORA_CR_LI_4_6           0x06
#define SX1280_LORA_CR_LI_4_7           0x07
#define SX1280_LORA_CR_LI_4_8           0x08

/* LoRa Packet Parameters */
#define SX1280_LORA_PACKET_EXPLICIT     0x00
#define SX1280_LORA_PACKET_IMPLICIT     0x80
#define SX1280_LORA_CRC_ON              0x20
#define SX1280_LORA_CRC_OFF             0x00
#define SX1280_LORA_IQ_NORMAL           0x40
#define SX1280_LORA_IQ_INVERTED         0x00

/**
 * @brief SX1280 radio configuration structure
 */
struct sx1280_config {
	uint32_t frequency_hz;
	uint8_t spreading_factor;
	uint8_t bandwidth;
	uint8_t coding_rate;
	int8_t tx_power_dbm;
	uint8_t preamble_length;
	uint8_t payload_length;
	bool crc_on;
	bool implicit_header;
};

/**
 * @brief SX1280 packet status structure
 */
struct sx1280_packet_status {
	int16_t rssi;          /* RSSI in dBm */
	int8_t snr;            /* SNR in dB */
	uint8_t sync_error;    /* Sync word error count */
};

/**
 * @brief Callback function for SX1280 interrupts
 *
 * @param dev Pointer to the device structure
 * @param irq_status IRQ status flags
 */
typedef void (*sx1280_irq_callback_t)(const struct device *dev, uint16_t irq_status);

/**
 * @brief Initialize the SX1280 radio
 *
 * @param dev Pointer to the device structure
 * @return 0 on success, negative errno code on failure
 */
int sx1280_init(const struct device *dev);

/**
 * @brief Configure the SX1280 radio
 *
 * @param dev Pointer to the device structure
 * @param config Pointer to configuration structure
 * @return 0 on success, negative errno code on failure
 */
int sx1280_configure(const struct device *dev, const struct sx1280_config *config);

/**
 * @brief Set the SX1280 to standby mode
 *
 * @param dev Pointer to the device structure
 * @return 0 on success, negative errno code on failure
 */
int sx1280_set_standby(const struct device *dev);

/**
 * @brief Set the SX1280 to transmit mode
 *
 * @param dev Pointer to the device structure
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return 0 on success, negative errno code on failure
 */
int sx1280_set_tx(const struct device *dev, uint16_t timeout_ms);

/**
 * @brief Set the SX1280 to receive mode
 *
 * @param dev Pointer to the device structure
 * @param timeout_ms Timeout in milliseconds (0xFFFF for continuous)
 * @return 0 on success, negative errno code on failure
 */
int sx1280_set_rx(const struct device *dev, uint16_t timeout_ms);

/**
 * @brief Write data to the SX1280 transmit buffer
 *
 * @param dev Pointer to the device structure
 * @param data Pointer to data buffer
 * @param len Length of data to write
 * @return 0 on success, negative errno code on failure
 */
int sx1280_write_buffer(const struct device *dev, const uint8_t *data, size_t len);

/**
 * @brief Read data from the SX1280 receive buffer
 *
 * @param dev Pointer to the device structure
 * @param data Pointer to data buffer
 * @param len Maximum length to read
 * @param bytes_read Pointer to store actual bytes read
 * @return 0 on success, negative errno code on failure
 */
int sx1280_read_buffer(const struct device *dev, uint8_t *data, size_t len, size_t *bytes_read);

/**
 * @brief Get the IRQ status from the SX1280
 *
 * @param dev Pointer to the device structure
 * @param irq_status Pointer to store IRQ status
 * @return 0 on success, negative errno code on failure
 */
int sx1280_get_irq_status(const struct device *dev, uint16_t *irq_status);

/**
 * @brief Clear the IRQ status on the SX1280
 *
 * @param dev Pointer to the device structure
 * @param irq_mask IRQ mask to clear
 * @return 0 on success, negative errno code on failure
 */
int sx1280_clear_irq_status(const struct device *dev, uint16_t irq_mask);

/**
 * @brief Get packet status (RSSI, SNR, etc.)
 *
 * @param dev Pointer to the device structure
 * @param status Pointer to store packet status
 * @return 0 on success, negative errno code on failure
 */
int sx1280_get_packet_status(const struct device *dev, struct sx1280_packet_status *status);

/**
 * @brief Register an IRQ callback
 *
 * @param dev Pointer to the device structure
 * @param callback Callback function
 * @return 0 on success, negative errno code on failure
 */
int sx1280_register_irq_callback(const struct device *dev, sx1280_irq_callback_t callback);

/**
 * @brief Get the length of the received packet
 *
 * @param dev Pointer to the device structure
 * @param length Pointer to store packet length
 * @return 0 on success, negative errno code on failure
 */
int sx1280_get_packet_length(const struct device *dev, uint8_t *length);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_RADIO_SX1280_H_ */
