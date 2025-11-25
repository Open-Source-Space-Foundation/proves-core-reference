// ======================================================================
// \title  MagnetorquerManager.hpp
// \author aychar
// \brief  hpp file for MagnetorquerManager component implementation class
// ======================================================================

#ifndef Drv_MagnetorquerManager_HPP
#define Drv_MagnetorquerManager_HPP

#include "FprimeZephyrReference/Components/Drv/MagnetorquerManager/MagnetorquerManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class MagnetorquerManager final : public MagnetorquerManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct MagnetorquerManager object
    MagnetorquerManager(const char* const compName  //!< The component name
    );

    //! Destroy MagnetorquerManager object
    ~MagnetorquerManager();

    //! Configure the DRV2605 device
    // Accept an array of six pointers to const device objects. The pointers themselves are const
    // to match callers that provide const device* const* types.
    void configure(const struct device* const devices[5]);

  private:
    //! Zephyr device to store initialized DRV2605 devices
    const struct device* m_devices[5];
    static struct drv2605_rom_data rom_data = {
        .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
        .library = DRV2605_LIBRARY_LRA,
        .seq_regs = {3, 3, 3, 3, 3, 3, 3, 3},
        .overdrive_time = 0,
        .sustain_pos_time = 0,
        .sustain_neg_time = 0,
        .brake_time = 0,
    };
    union drv2605_config_data config_data = {};
    config_data.rom_data = &rom_data;

    // Command handlers updated to accept a face index (0..5)
    void SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) override;
    void run_handler(FwIndexType portNum, U32 context) override;
    bool enabled = false;
};

}  // namespace Drv

#endif
