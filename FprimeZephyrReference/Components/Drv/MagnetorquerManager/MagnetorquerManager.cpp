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

void MagnetorquerManager ::configure(const std::map<std::string, const struct device*>& devices) {
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
    for (const auto& [key, device] : devices) {
        this->m_devices[key] = device;
        this->enabled_faces[key] = false;

        int ret = device_init(device);
        if (ret != 0) {
            this->log_WARNING_HI_DeviceNotInitialized(Fw::LogStringArg(key.c_str()));
            continue;
        }

        drv2605_haptic_config(device, DRV2605_HAPTICS_SOURCE_ROM, &this->config_data);
    }
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------
void MagnetorquerManager ::run_handler(FwIndexType portNum, U32 context) {
    if (this->enabled) {
        for (const auto& [key, device] : this->m_devices) {
            if (this->enabled_faces[key]) {
                if (!device_is_ready(device)) {
                    this->log_WARNING_HI_DeviceNotReady(Fw::LogStringArg(key.c_str()));
                    return;
                }
                haptics_start_output(device);
            }
        }
    }
}

void MagnetorquerManager ::SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) {
    this->enabled = true;

    // Loop through 10 times to match InputArray size in fpp type.
    for (int i = 0; i < 10; i++) {
        const Drv::InputStruct& entry = value[i];
        std::string key = entry.get_key().toChar();
        bool enabled = entry.get_value();

        auto it = this->m_devices.find(key);
        if (it == this->m_devices.end()) {
            this->log_WARNING_HI_InvalidFace(Fw::LogStringArg(key.c_str()));
            continue;
        }
        this->enabled_faces[key] = enabled;
    }
}

void MagnetorquerManager ::SetDisabled_handler(const FwIndexType portNum) {
    this->enabled = false;

    for (const auto& [key, device] : this->m_devices) {
        if (!device_is_ready(device)) {
            this->log_WARNING_HI_DeviceNotReady(Fw::LogStringArg(key.c_str()));
            continue;
        }

        haptics_stop_output(device);
        this->enabled_faces[key] = false;
    }
}

}  // namespace Drv
