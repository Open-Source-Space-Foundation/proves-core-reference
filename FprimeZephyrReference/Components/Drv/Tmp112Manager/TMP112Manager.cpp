// ======================================================================
// \title  TMP112Manager.cpp
// \brief  cpp file for TMP112Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Tmp112Manager/TMP112Manager.hpp"

#include <Fw/Types/Assert.hpp>

#include <zephyr/sys/printk.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TMP112Manager ::TMP112Manager(const char* const compName) : TMP112ManagerComponentBase(compName) {}

TMP112Manager ::~TMP112Manager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void TMP112Manager::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void TMP112Manager ::init_handler(FwIndexType portNum, Fw::Success& condition) {
    int ret = device_init(this->m_dev);
    // do i need to start the mux first?
    // if (ret != 0 && ret != -EALREADY) {
    // if (ret == -EALREADY) {
    //     // Device already initialized - treat as success
    //     this->log_WARNING_HI_DeviceAlreadyInitialized();
    //     return;
    // }
    if (ret != 0) {
        // Emit an error? or ignore because face is off? Talk to load manager to see if face is on before erroring?
        this->log_WARNING_HI_DeviceInitFailed(ret);
        condition = Fw::Success::FAILURE;

        // Even after a failure to init, the device will stay in an initialized state
        // so we need to deinit to allow future init attempts
        int deinit_ret = device_deinit(this->m_dev);  // TODO: Check output?
        if (deinit_ret != 0) {
            this->log_WARNING_HI_DeviceDeinitFailed(deinit_ret);
            return;
        }
        this->log_WARNING_HI_DeviceDeinitFailed_ThrottleClear();
        return;
    }

    this->log_WARNING_HI_DeviceInitFailed_ThrottleClear();
    condition = Fw::Success::SUCCESS;
}

F64 TMP112Manager ::temperatureGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        printk("[TMP112Manager] Device not ready: %s (ptr: %p)\n", this->m_dev->name, this->m_dev);
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value temp;

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (rc != 0) {
        // Sensor fetch failed - return 0 to indicate error
        printk("[TMP112Manager] sensor_sample_fetch_chan failed for %s: rc=%d\n", this->m_dev->name, rc);
        return 0;
    }

    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (rc != 0) {
        // Channel get failed - return 0 to indicate error
        printk("[TMP112Manager] sensor_channel_get failed for %s: rc=%d\n", this->m_dev->name, rc);
        return 0;
    }

    this->tlmWrite_Temperature(sensor_value_to_double(&temp));

    return sensor_value_to_double(&temp);
}

}  // namespace Drv
