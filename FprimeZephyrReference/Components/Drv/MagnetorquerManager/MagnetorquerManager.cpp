// ======================================================================
// \title  MagnetorquerManager.cpp
// \author aychar
// \brief  cpp file for MagnetorquerManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/MagnetorquerManager/MagnetorquerManager.hpp"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MagnetorquerManager ::MagnetorquerManager(const char* const compName) : MagnetorquerManagerComponentBase(compName) {}

MagnetorquerManager ::~MagnetorquerManager() {}

void MagnetorquerManager ::configure(const struct device* const devices[5]) {
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
    this->config_data.rom_data = &rom_data;

    // Configure each device
    for (int i = 0; i < 5; ++i) {
        this->m_devices[i] = devices[i];
        drv2605_haptic_config(this->m_devices[i], DRV2605_HAPTICS_SOURCE_ROM, &this->config_data);
    }
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------
void MagnetorquerManager ::run_handler(FwIndexType portNum, U32 context) {
    if (this->enabled) {
        for (int i = 0; i < 5; ++i) {
            const struct device* dev = this->m_devices[i];
            if (!device_is_ready(dev)) {
                this->log_WARNING_HI_DeviceNotReady();
                return;
            }
            haptics_start_output(dev);
        }
    } else {
        for (int i = 0; i < 5; ++i) {
            const struct device* dev = this->m_devices[i];
            if (!device_is_ready(dev)) {
                this->log_WARNING_HI_DeviceNotReady();
                return;
            }
            haptics_stop_output(dev);
        }
    }
}

void MagnetorquerManager ::SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) {
    // TODO(hrfarmer): Once its possible to properly interact with the DRV2605, I'll figure out how to
    // determine how the passed in amps should translate to a specific pattern(s) that should be ran.
    return;
}

}  // namespace Drv
