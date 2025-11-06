// ======================================================================
// \title  MagnetorquerManager.hpp
// \author aychar
// \brief  hpp file for MagnetorquerManager component implementation class
// ======================================================================

#ifndef Drv_MagnetorquerManager_HPP
#define Drv_MagnetorquerManager_HPP

#include "FprimeZephyrReference/Components/Drv/MagnetorquerManager/MagnetorquerManagerComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/haptics/drv2605.h>

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
    void configure(const struct device* dev);

  private:
    //! Zephyr device to store initialized DRV2605
    const struct device* m_dev;
    struct drv2605_rom_data rom = {.library = DRV2605_LIBRARY_TS2200_A, .seq_regs = {47, 0, 0, 0, 0, 0, 0, 0}};

    void START_PLAYBACK_TEST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void START_PLAYBACK_TEST2_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
};

}  // namespace Drv

#endif
