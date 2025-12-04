// ======================================================================
// \title  Drv2605Manager.hpp
// \author aychar
// \brief  hpp file for Drv2605Manager component implementation class
// ======================================================================

#ifndef Drv_Drv2605Manager_HPP
#define Drv_Drv2605Manager_HPP

#include "FprimeZephyrReference/Components/Drv/Drv2605Manager/Drv2605ManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/drivers/sensor.h>

namespace Drv {

class Drv2605Manager final : public Drv2605ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Drv2605Manager object
    Drv2605Manager(const char* const compName  //!< The component name
    );

    //! Destroy Drv2605Manager object
    ~Drv2605Manager();

    //! Configure the DRV2605 device
    void configure(const struct device* dev);

  private:
    //! Port to initialize the DRV2605 device
    //! Must be called and complete successfully at least one time before temperature can be read
    void init_handler(FwIndexType portNum,    //!< The port number
                      Fw::Success& condition  //!< Condition success/failure
                      ) override;

    //! Port to trigger the DRV2605 device to run the defined handler
    bool triggerDevice_handler(FwIndexType portNum  //!< The port number
                               ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    const struct device* m_dev;
    union drv2605_config_data m_config_data;
};

}  // namespace Drv

#endif
