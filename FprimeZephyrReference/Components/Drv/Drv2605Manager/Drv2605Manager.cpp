// ======================================================================
// \title  Drv2605Manager.cpp
// \author aychar
// \brief  cpp file for Drv2605Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Drv2605Manager/Drv2605Manager.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Drv2605Manager ::Drv2605Manager(const char* const compName) : Drv2605ManagerComponentBase(compName) {}

Drv2605Manager ::~Drv2605Manager() {}

void Drv2605Manager ::configure(const struct device* dev) {
    this->m_dev = dev;
}

void Drv2605Manager ::init_handler(FwIndexType portNum, Fw::Success& condition) {
    if (!this->m_dev) {
        condition = Fw::Success::FAILURE;
        this->log_WARNING_HI_DeviceNil();
        return;
    }
    this->log_WARNING_HI_DeviceNil_ThrottleClear();

    if (!this->m_dev->state) {
        condition = Fw::Success::FAILURE;
        this->log_WARNING_HI_DeviceStateNil();
        return;
    }
    this->log_WARNING_HI_DeviceStateNil_ThrottleClear();

    // Reset the device initialization state to allow device_init to run again.
    this->m_dev->state->initialized = false;

    int rc = device_init(this->m_dev);
    if (rc < 0) {
        condition = Fw::Success::FAILURE;
        this->log_WARNING_HI_DeviceInitFailed(rc);
        return;
    }
    this->log_WARNING_HI_DeviceInitFailed_ThrottleClear();

    // Configure DRV2605 config struct
    static struct drv2605_rom_data rom_data = {
        .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
        .library = DRV2605_LIBRARY_LRA,
        .seq_regs = {3, 3, 3, 3, 3, 3, 3, 3},
        .overdrive_time = 0,
        .sustain_pos_time = 0,
        .sustain_neg_time = 0,
        .brake_time = 0,
    };
    this->m_config_data.rom_data = &rom_data;
    drv2605_haptic_config(this->m_dev, DRV2605_HAPTICS_SOURCE_ROM, &this->m_config_data);

    condition = Fw::Success::SUCCESS;
}

bool Drv2605Manager ::triggerDevice_handler(FwIndexType portNum) {
    int res = haptics_start_output(this->m_dev);
    return res != 0 ? true : false;
}

}  // namespace Drv
