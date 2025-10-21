// ======================================================================
// \title  Lis2mdlManager.hpp
// \brief  hpp file for Lis2mdlManager component implementation class
// ======================================================================

#ifndef Components_Lis2mdlManager_HPP
#define Components_Lis2mdlManager_HPP

// clang-format off
// Keep the includes in this order
#include "FprimeZephyrReference/Components/Drv/Lis2mdlManager/Lis2mdlManagerComponentAc.hpp"
#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"
// clang-format on

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class Lis2mdlManager final : public Lis2mdlManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Lis2mdlManager object
    Lis2mdlManager(const char* const compName);

    //! Destroy Lis2mdlManager object
    ~Lis2mdlManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Get the magnetic field reading from the LIS2MDL sensor
    Drv::MagneticField magneticFieldGet_handler(const FwIndexType portNum  //!< The port number
                                                ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LIS2MDL sensor
    const struct device* dev;
};

}  // namespace Drv

#endif
