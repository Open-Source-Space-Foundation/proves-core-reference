// ======================================================================
// \title  MagnetorquerManager.cpp
// \author aychar
// \brief  cpp file for MagnetorquerManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/MagnetorquerManager/MagnetorquerManager.hpp"

#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/kernel.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MagnetorquerManager ::MagnetorquerManager(const char* const compName) : MagnetorquerManagerComponentBase(compName) {}

MagnetorquerManager ::~MagnetorquerManager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void MagnetorquerManager ::configure(const struct device* const devices[6]) {
    for (int i = 0; i < 6; ++i) {
        this->m_devices[i] = devices[i];
    }
}

void MagnetorquerManager ::START_PLAYBACK_TEST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) {
    // Validate face index (0..5)
    if (faceIdx >= 6) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    const struct device* dev = this->m_devices[faceIdx];
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    drv2605_haptic_config(dev, DRV2605_HAPTICS_SOURCE_ROM, (union drv2605_config_data*)&this->rom);
}

void MagnetorquerManager ::START_PLAYBACK_TEST2_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) {
    // Validate face index (0..5)
    if (faceIdx >= 6) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    const struct device* dev = this->m_devices[faceIdx];
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    struct drv2605_rom_data rom2 = {.library = DRV2605_LIBRARY_TS2200_A, .seq_regs = {50, 0, 0, 0, 0, 0, 0, 0}};
    drv2605_haptic_config(dev, DRV2605_HAPTICS_SOURCE_ROM, (union drv2605_config_data*)&rom2);
}

}  // namespace Drv
