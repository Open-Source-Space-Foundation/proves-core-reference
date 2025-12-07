// ======================================================================
// \title  SBand.cpp
// \author jrpear
// \brief  cpp file for SBand component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/SBand/SBand.hpp"

#include <Fw/Buffer/Buffer.hpp>
#include <Fw/Logger/Logger.hpp>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/radio/sx1280.h>

namespace Components {

// Device tree reference
#define SBAND_NODE DT_NODELABEL(sband0)

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SBand ::SBand(const char* const compName)
    : SBandComponentBase(compName), m_device(DEVICE_DT_GET(SBAND_NODE)) {}

SBand ::~SBand() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

// IRQ callback for SX1280 driver
static void sband_irq_callback(const struct device *dev, uint16_t irq_status) {
    // This will be called from interrupt context
    // We need to defer processing to the run handler
}

void SBand ::run_handler(FwIndexType portNum, U32 context) {
    // Only process if radio is configured
    if (!m_configured) {
        return;
    }

    // Check IRQ status
    uint16_t irq_status;
    if (sx1280_get_irq_status(m_device, &irq_status) != 0) {
        return;
    }

    // Only process if RX_DONE
    if (irq_status & SX1280_IRQ_RX_DONE) {
        // Clear the IRQ
        sx1280_clear_irq_status(m_device, SX1280_IRQ_RX_DONE);

        // Process received data
        uint8_t data[256] = {0};
        size_t len = 0;
        uint8_t packet_len;

        int ret = sx1280_get_packet_length(m_device, &packet_len);
        if (ret != 0) {
            this->log_WARNING_HI_RadioLibFailed(ret);
            return;
        }

        ret = sx1280_read_buffer(m_device, data, sizeof(data), &len);
        if (ret != 0) {
            this->log_WARNING_HI_RadioLibFailed(ret);
        } else {
            Fw::Buffer buffer = this->allocate_out(0, static_cast<FwSizeType>(len));
            if (buffer.isValid()) {
                (void)::memcpy(buffer.getData(), data, len);
                ComCfg::FrameContext frameContext;
                this->dataOut_out(0, buffer, frameContext);

                // Log RSSI and SNR for received packet
                struct sx1280_packet_status pkt_status;
                if (sx1280_get_packet_status(m_device, &pkt_status) == 0) {
                    this->tlmWrite_LastRssi(static_cast<float>(pkt_status.rssi));
                    this->tlmWrite_LastSnr(static_cast<float>(pkt_status.snr));
                }

                // Clear throttled warnings on success
                this->log_WARNING_HI_RadioLibFailed_ThrottleClear();
                this->log_WARNING_HI_AllocationFailed_ThrottleClear();
            } else {
                this->log_WARNING_HI_AllocationFailed(static_cast<FwSizeType>(len));
            }
        }

        // Re-enable receive mode
        ret = sx1280_set_rx(m_device, 0xFFFF); // Continuous RX
        if (ret != 0) {
            this->log_WARNING_HI_RadioLibFailed(ret);
        }
    }
}

void SBand ::deferredRxHandler_internalInterfaceHandler() {
    // No longer used with Zephyr driver
}

void SBand ::deferredTxHandler_internalInterfaceHandler(const Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Fw::Success returnStatus = Fw::Success::FAILURE;

    // Enable transmit mode
    Status status = this->enableTx();
    if (status == Status::SUCCESS) {
        // Write data to buffer
        int ret = sx1280_write_buffer(m_device, data.getData(), data.getSize());
        if (ret != 0) {
            this->log_WARNING_HI_RadioLibFailed(ret);
            returnStatus = Fw::Success::FAILURE;
        } else {
            // Start transmission
            ret = sx1280_set_tx(m_device, 100); // 100ms timeout
            if (ret != 0) {
                this->log_WARNING_HI_RadioLibFailed(ret);
                returnStatus = Fw::Success::FAILURE;
            } else {
                // Wait for TX done (polling for now)
                uint16_t irq_status;
                int timeout = 1000; // 1 second
                while (timeout > 0) {
                    if (sx1280_get_irq_status(m_device, &irq_status) == 0) {
                        if (irq_status & SX1280_IRQ_TX_DONE) {
                            sx1280_clear_irq_status(m_device, SX1280_IRQ_TX_DONE);
                            returnStatus = Fw::Success::SUCCESS;
                            this->log_WARNING_HI_RadioLibFailed_ThrottleClear();
                            break;
                        }
                    }
                    k_msleep(1);
                    timeout--;
                }
                
                if (timeout == 0) {
                    this->log_WARNING_HI_RadioLibFailed(-ETIMEDOUT);
                    returnStatus = Fw::Success::FAILURE;
                }
            }
        }
    }

    // Return buffer and status (need mutable copy for dataReturnOut_out)
    Fw::Buffer mutableData = data;
    this->dataReturnOut_out(0, mutableData, context);
    this->comStatusOut_out(0, returnStatus);

    // Re-enable RX mode after transmission
    status = this->enableRx();
    // enableRx will log RadioLibFailed internally if it fails
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Only process if radio is configured
    if (!m_configured) {
        this->log_WARNING_HI_RadioNotConfigured();
        Fw::Success failureStatus = Fw::Success::FAILURE;
        this->dataReturnOut_out(0, data, context);
        this->comStatusOut_out(0, failureStatus);
        return;
    }

    // Queue deferred handler to perform transmission
    this->deferredTxHandler_internalInterfaceInvoke(data, context);
}

void SBand ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Deallocate the buffer
    this->deallocate_out(0, data);
}

SBand::Status SBand ::enableRx() {
    // Set standby first
    int ret = sx1280_set_standby(m_device);
    if (ret != 0) {
        this->log_WARNING_HI_RadioLibFailed(ret);
        return Status::ERROR;
    }

    // Start receive mode
    ret = sx1280_set_rx(m_device, 0xFFFF); // Continuous RX
    if (ret != 0) {
        this->log_WARNING_HI_RadioLibFailed(ret);
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

SBand::Status SBand ::enableTx() {
    // Set standby first
    int ret = sx1280_set_standby(m_device);
    if (ret != 0) {
        this->log_WARNING_HI_RadioLibFailed(ret);
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

SBand::Status SBand ::configure_radio() {
    // Initialize the driver (already done in driver init, but ensure it's ready)
    if (!device_is_ready(m_device)) {
        this->log_WARNING_HI_RadioLibFailed(-ENODEV);
        return Status::ERROR;
    }

    // Configure radio parameters matching CircuitPython defaults
    struct sx1280_config config;
    config.frequency_hz = 2400000000; // 2.4 GHz
    config.spreading_factor = SX1280_LORA_SF7;
    config.bandwidth = SX1280_LORA_BW_406;
    config.coding_rate = SX1280_LORA_CR_4_5;
    config.tx_power_dbm = 13; // Max power
    config.preamble_length = 12;
    config.payload_length = 255;
    config.crc_on = true;
    config.implicit_header = false;

    int ret = sx1280_configure(m_device, &config);
    if (ret != 0) {
        this->log_WARNING_HI_RadioLibFailed(ret);
        return Status::ERROR;
    }

    // Register IRQ callback
    ret = sx1280_register_irq_callback(m_device, sband_irq_callback);
    if (ret != 0) {
        this->log_WARNING_HI_RadioLibFailed(ret);
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

SBand::Status SBand ::configureRadio() {
    Status config_status = this->configure_radio();
    if (config_status != Status::SUCCESS) {
        // configure_radio logs RadioLibFailed internally
        return Status::ERROR;
    }

    // Mark as configured
    m_configured = true;

    // Enable RX mode
    Status rx_status = this->enableRx();
    if (rx_status != Status::SUCCESS) {
        // enableRx logs RadioLibFailed internally
        return Status::ERROR;
    }

    // Send success status
    Fw::Success status = Fw::Success::SUCCESS;
    this->comStatusOut_out(0, status);

    return Status::SUCCESS;
}

}  // namespace Components
