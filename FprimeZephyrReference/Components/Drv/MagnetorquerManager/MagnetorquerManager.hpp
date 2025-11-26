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
    // Accept an array of five pointers to const device objects. The pointers themselves are const
    // to match callers that provide const device* const* types.
    void configure(const struct device* const devices[5]);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;

    //! Zephyr device to store initialized DRV2605 devices
    const struct device* m_devices[5];
    union drv2605_config_data config_data;

    // Port handler implementations
    void SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) override;
    void SetDisabled_handler(const FwIndexType portNum) override;

    // Command handler implementations
    void EnableMagnetorquers_cmdHandler(const FwOpcodeType opCode,
                                        const U32 cmdSeq,
                                        const Drv::InputArray& inputArray) override;

    // Local variables
    bool enabled = false;
    bool enabled_faces[5] = {false, false, false, false, false};
};
}  // namespace Drv

#endif
