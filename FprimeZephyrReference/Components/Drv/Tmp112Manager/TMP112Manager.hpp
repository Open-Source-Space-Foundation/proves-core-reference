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
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the TMP112 device
    void configure(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for loadSwitchStateChanged
    //!
    //! Port to initialize and deinitialize the TMP112 device on load switch state change
    Fw::Success loadSwitchStateChanged_handler(FwIndexType portNum,  //!< The port number
                                               const Fw::On& state) override;

    //! Handler implementation for temperatureGet
    //!
    //! Port to read the temperature in degrees Celsius
    F64 temperatureGet_handler(FwIndexType portNum,  //!< The port number
                               Fw::Success& condition) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Initialize the TMP112 device
    Fw::Success initializeDevice();

    //! Deinitialize the TMP112 device
    Fw::Success deinitializeDevice();

    //! Check if the TMP112 device is initialized
    bool isDeviceInitialized();

    //! Check if the load switch is ready (on and timeout passed)
    bool loadSwitchReady();

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized TMP112 sensor
    const struct device* m_dev;

    //! TCA health state
    Fw::Health m_tca_state = Fw::Health::FAILED;

    //! MUX health state
    Fw::Health m_mux_state = Fw::Health::FAILED;

    //! Load switch state
    Fw::On m_load_switch_state = Fw::On::OFF;

    //! Load switch on timeout
    //! Time when we can consider the load switch to be fully on (giving time for power to normalize)
    Fw::Time m_load_switch_on_timeout;
};

}  // namespace Drv

#endif
