// ======================================================================
// \title  GenericDeviceMonitor.hpp
// \author nate
// \brief  hpp file for GenericDeviceMonitor component implementation class
// ======================================================================

#ifndef Components_GenericDeviceMonitor_HPP
#define Components_GenericDeviceMonitor_HPP

#include "FprimeZephyrReference/Components/GenericDeviceMonitor/GenericDeviceMonitorComponentAc.hpp"
#include <zephyr/device.h>

namespace Components {

class GenericDeviceMonitor final : public GenericDeviceMonitorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct GenericDeviceMonitor object
    GenericDeviceMonitor(const char* const compName  //!< The component name
    );

    //! Destroy GenericDeviceMonitor object
    ~GenericDeviceMonitor();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the zephyr mux channel device
    void configure(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for healthGet
    //!
    //! Port receiving calls to request device health
    Fw::Health healthGet_handler(FwIndexType portNum  //!< The port number
                                 ) override;

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized mux channel
    const struct device* m_dev;
};

}  // namespace Components

#endif
