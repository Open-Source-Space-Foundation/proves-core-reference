// ======================================================================
// \title  TMP112Manager.hpp
// \brief  hpp file for TMP112Manager component implementation class
// ======================================================================

#ifndef Drv_TMP112Manager_HPP
#define Drv_TMP112Manager_HPP

#include "FprimeZephyrReference/Components/Drv/Tmp112Manager/TMP112ManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
namespace Drv {

class TMP112Manager final : public TMP112ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TMP112Manager object
    TMP112Manager(const char* const compName  //!< The component name
    );

    //! Destroy TMP112Manager object
    ~TMP112Manager();

  public:
    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Configure the TMP112 device
    void configure(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for init
    //!
    //! Port to initialize the TMP112 device
    //! Must be called and complete successfully at least one time before temperature can be read
    void init_handler(FwIndexType portNum,    //!< The port number
                      Fw::Success& condition  //!< Condition success/failure
                      ) override;

    //! Handler implementation for temperatureGet
    //!
    //! Port to read the temperature in degrees Celsius
    F64 temperatureGet_handler(FwIndexType portNum  //!< The port number
                               ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized TMP112 sensor
    const struct device* m_dev;

    //! Initialization state
    bool m_initialized = false;
};

}  // namespace Drv

#endif
