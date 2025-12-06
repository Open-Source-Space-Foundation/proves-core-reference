// ======================================================================
// \title  MagnetorquerManager.hpp
// \author aychar
// \brief  hpp file for MagnetorquerManager component implementation class
// ======================================================================

#ifndef Components_MagnetorquerManager_HPP
#define Components_MagnetorquerManager_HPP

#include <map>
#include <string>

#include "FprimeZephyrReference/Components/MagnetorquerManager/MagnetorquerManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Components {

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
    void configure();

  private:
    // Port handler implementations
    void run_handler(FwIndexType portNum, U32 context) override;
    void SetMagnetorquers_handler(const FwIndexType portNum, const Components::InputArray& value) override;
    void SetDisabled_handler(const FwIndexType portNum) override;

    //! Zephyr device to store initialized DRV2605 devices
    std::map<std::string, const struct device*> m_devices;

    // Local variables

    // NOTE: The order of the faces here matter, their index will map to a specific port
    std::string faces[5] = {"X+", "X-", "Y+", "Y-", "Z+"};
    std::map<std::string, bool> enabled_faces;
    bool enabled = false;
};
}  // namespace Components

#endif
