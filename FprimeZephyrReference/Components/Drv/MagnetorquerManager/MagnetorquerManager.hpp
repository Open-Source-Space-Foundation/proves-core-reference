// ======================================================================
// \title  MagnetorquerManager.hpp
// \author aychar
// \brief  hpp file for MagnetorquerManager component implementation class
// ======================================================================

#ifndef Drv_MagnetorquerManager_HPP
#define Drv_MagnetorquerManager_HPP

#include <map>
#include <string>

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
    void configure(const std::map<std::string, const struct device*>& devices);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;

    //! Zephyr device to store initialized DRV2605 devices
    std::map<std::string, const struct device*> m_devices;

    union drv2605_config_data config_data;

    // Port handler implementations
    void SetMagnetorquers_handler(const FwIndexType portNum, const Drv::InputArray& value) override;
    void SetDisabled_handler(const FwIndexType portNum) override;

    // Local variables
    bool enabled = false;
    std::map<std::string, bool> enabled_faces;
};
}  // namespace Drv

#endif
