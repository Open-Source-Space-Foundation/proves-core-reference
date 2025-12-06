// ======================================================================
// \title  MagnetorquerManager.cpp
// \author aychar
// \brief  cpp file for MagnetorquerManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/MagnetorquerManager/MagnetorquerManager.hpp"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MagnetorquerManager ::MagnetorquerManager(const char* const compName) : MagnetorquerManagerComponentBase(compName) {}

MagnetorquerManager ::~MagnetorquerManager() {}

void MagnetorquerManager ::configure() {
    // Manually enable load switches
    for (int i = 0; i < 5; i++) {
        this->loadSwitchTurnOn_out(i);
    }

    Fw::Success condition;
    // Initialize enabled_faces
    for (int i = 0; i < 5; i++) {
        this->initDevice_out(i, condition);
        this->enabled_faces[this->faces[i]] = false;
    }
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------
void MagnetorquerManager ::run_handler(FwIndexType portNum, U32 context) {
    if (this->enabled) {
        for (int i = 0; i < 5; i++) {
            if (this->enabled_faces[this->faces[i]]) {
                this->triggerDevice_out(i);
            }
        }
    }
}

void MagnetorquerManager ::SetMagnetorquers_handler(const FwIndexType portNum, const Components::InputArray& value) {
    this->enabled = true;

    // Loop through 5 times to match InputArray size in fpp type.
    for (int i = 0; i < 5; i++) {
        const Components::InputStruct& entry = value[i];
        std::string key = entry.get_key().toChar();
        bool enabled = entry.get_value();

        auto it = this->enabled_faces.find(key);
        if (it == this->enabled_faces.end()) {
            this->log_WARNING_HI_InvalidFace(Fw::LogStringArg(key.c_str()));
            continue;
        }
        this->enabled_faces[key] = enabled;
    }
}

void MagnetorquerManager ::SetDisabled_handler(const FwIndexType portNum) {
    this->enabled = false;

    for (int i = 0; i < 5; i++) {
        this->enabled_faces[this->faces[i]] = false;
    }
}

}  // namespace Components
