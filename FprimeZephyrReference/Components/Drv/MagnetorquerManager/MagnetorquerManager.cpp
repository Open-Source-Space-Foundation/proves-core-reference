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

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------
void MagnetorquerManager ::run_handler(FwIndexType portNum, U32 context) {
    const struct device* dev = this->m_devices[0];
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }
    if (this->enabled) {
        static struct drv2605_rom_data rom_data = {
            .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
            .library = DRV2605_LIBRARY_LRA,
            .seq_regs = {1, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 2, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 3,
                         DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 4},
            .overdrive_time = 0,
            .sustain_pos_time = 0,
            .sustain_neg_time = 0,
            .brake_time = 0,
        };
        union drv2605_config_data config_data = {};

        config_data.rom_data = &rom_data;
        this->log_ACTIVITY_HI_RetResult(0);
        int ret = drv2605_haptic_config(dev, DRV2605_HAPTICS_SOURCE_ROM, &config_data);
        this->log_ACTIVITY_HI_RetResult(ret);
    }
    // this->enabled ? haptics_start_output(dev) : haptics_stop_output(dev);
}

void MagnetorquerManager ::SET_ENABLED_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool enable) {
    const struct device* dev = this->m_devices[0];
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }
    static struct drv2605_rom_data rom_data = {
        .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
        .library = DRV2605_LIBRARY_LRA,
        .seq_regs = {1, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(500), 2, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(500), 3,
                     DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(500), 4},
        .overdrive_time = 0,
        .sustain_pos_time = 0,
        .sustain_neg_time = 0,
        .brake_time = 0,
    };
    union drv2605_config_data config_data = {};

    config_data.rom_data = &rom_data;
    int ret = drv2605_haptic_config(dev, DRV2605_HAPTICS_SOURCE_ROM, &config_data);
    haptics_start_output(dev);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MagnetorquerManager ::configure(const struct device* const devices[5]) {
    for (int i = 0; i < 5; ++i) {
        this->m_devices[i] = devices[i];
    }
}

void MagnetorquerManager ::START_PLAYBACK_TEST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) {
    // Validate face index (0..4)
    if (faceIdx >= 5) {
        this->log_WARNING_LO_InvalidFaceIndex();
        return;
    }

    const struct device* dev = this->m_devices[faceIdx];
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    static struct drv2605_rom_data rom_data = {
        .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
        .library = DRV2605_LIBRARY_LRA,
        .seq_regs = {1, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 2, DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 3,
                     DRV2605_WAVEFORM_SEQUENCER_WAIT_MS(100), 4},
        .overdrive_time = 0,
        .sustain_pos_time = 0,
        .sustain_neg_time = 0,
        .brake_time = 0,
    };
    union drv2605_config_data config_data = {};

    config_data.rom_data = &rom_data;

    for (int i = 0; i < 10; ++i) {
        haptics_start_output(dev);
        k_sleep(K_MSEC(1000));
        haptics_stop_output(dev);
        k_sleep(K_MSEC(500));
    }
    // int ret = drv2605_haptic_config(dev, DRV2605_HAPTICS_SOURCE_ROM, &config_data);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MagnetorquerManager ::START_PLAYBACK_TEST2_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) {
    // Validate face index (0..4)
    if (faceIdx >= 5) {
        this->log_WARNING_LO_InvalidFaceIndex();
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

void MagnetorquerManager ::SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) {
    // TODO(hrfarmer): Once its possible to properly interact with the DRV2605, I'll figure out how to
    // determine how the passed in amps should translate to a specific pattern(s) that should be ran.
    return;
}

}  // namespace Drv
