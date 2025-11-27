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
    void configure(const struct device* const devices[6]);

  private:
    //! Zephyr device to store initialized DRV2605
    const struct device* m_devices[6];
    struct drv2605_rom_data rom = {.library = DRV2605_LIBRARY_TS2200_A, .seq_regs = {47, 0, 0, 0, 0, 0, 0, 0}};

    // Command handlers updated to accept a face index (0..5)
    void START_PLAYBACK_TEST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) override;
    void START_PLAYBACK_TEST2_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 faceIdx) override;
};

}  // namespace Drv

#endif
