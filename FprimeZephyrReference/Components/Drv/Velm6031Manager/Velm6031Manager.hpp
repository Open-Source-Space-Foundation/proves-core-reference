// ======================================================================
// \title  Velm6031Manager.hpp
// \brief  hpp file for Velm6031Manager component implementation class
// ======================================================================

#ifndef Components_Velm6031Manager_HPP
#define Components_Velm6031Manager_HPP

#include <stdio.h>

#include "FprimeZephyrReference/Components/Drv/Velm6031Manager/Velm6031ManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/veml6031.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>

namespace Drv {

class Velm6031Manager final : public Velm6031ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Velm6031Manager object
    Velm6031Manager(const char* const compName  //!< The component name
    );

    //! Destroy Velm6031Manager object
    ~Velm6031Manager();

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

    //! Handler implementation for ambientLightGet
    //!
    //! Port to read the ambient illuminance in visible spectrum, in lux
    F32 ambientLightGet_handler(FwIndexType portNum,  //!< The port number
                                Fw::Success& condition) override;

    //! Handler implementation for infraRedLightGet
    //!
    //! Port to read the illuminance in infra-red spectrum, in lux
    F32 infraRedLightGet_handler(FwIndexType portNum,  //!< The port number
                                 Fw::Success& condition) override;

    //! Handler implementation for loadSwitchStateChanged
    //!
    //! Port to initialize and deinitialize the VELM6031 device on load switch state change
    Fw::Success loadSwitchStateChanged_handler(FwIndexType portNum,  //!< The port number
                                               const Fw::On& state) override;

    //! Handler implementation for visibleLightGet
    //!
    //! Port to read the illuminance in visible spectrum, in lux
    //! This channel represents the raw measurement counts provided by the sensor ALS register.
    //! It is useful for estimating good values for integration time, effective photodiode size
    //! and gain attributes in fetch and get mode.
    F32 visibleLightGet_handler(FwIndexType portNum,  //!< The port number
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

    //! Set the integration time attribute for the VELM6031 sensor
    Fw::Success setIntegrationTimeAttribute(sensor_channel chan);

    //! Set the gain attribute for the VELM6031 sensor
    Fw::Success setGainAttribute(sensor_channel chan);

    //! Set the div4 attribute for the VELM6031 sensor
    Fw::Success setDiv4Attribute(sensor_channel chan);

    //! Set all sensor attributes for the VELM6031 sensor
    Fw::Success configureSensorAttributes(sensor_channel chan);

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
