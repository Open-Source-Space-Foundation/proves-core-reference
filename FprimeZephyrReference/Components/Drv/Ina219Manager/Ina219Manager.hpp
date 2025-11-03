// ======================================================================
// \title  Ina219Manager.hpp
// \author nate
// \brief  hpp file for Ina219Manager component implementation class
// ======================================================================

#ifndef Drv_Ina219Manager_HPP
#define Drv_Ina219Manager_HPP

#include "FprimeZephyrReference/Components/Drv/Ina219Manager/Ina219ManagerComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
namespace Drv {

class Ina219Manager final : public Ina219ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Ina219Manager object
    Ina219Manager(const char* const compName  //!< The component name
    );

    //! Destroy Ina219Manager object
    ~Ina219Manager();

    void configure(const struct device *dev); // helper function to configure device for the class

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for currentGet
    //!
    //! Port to read the current in amps
    F64 currentGet_handler(FwIndexType portNum  //!< The port number
                           ) override;

    //! Handler implementation for powerGet
    //!
    //! Port to read the power in watts
    F64 powerGet_handler(FwIndexType portNum  //!< The port number
                         ) override;

    //! Handler implementation for voltageGet
    //!
    //! Port to read the voltage in volts
    F64 voltageGet_handler(FwIndexType portNum  //!< The port number
                           ) override;

    
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LIS2MDL sensor
    const struct device* m_dev;                    
};

}  // namespace Drv

#endif
