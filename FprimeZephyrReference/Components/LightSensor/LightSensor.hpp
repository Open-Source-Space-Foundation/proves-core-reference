// ======================================================================
// \title  LightSensor.hpp
// \author robertpendergrast
// \brief  hpp file for LightSensor component implementation class
// ======================================================================

#ifndef Components_LightSensor_HPP
#define Components_LightSensor_HPP

#include "FprimeZephyrReference/Components/LightSensor/LightSensorComponentAc.hpp"

#include <zephyr/kernel.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/drivers/sensor/veml6031.h>

namespace Components {

class LightSensor final : public LightSensorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct LightSensor object
    LightSensor(const char* const compName  //!< The component name
    );

    //! Destroy LightSensor object
    ~LightSensor();


    void ReadData();

    void ConfigureSensor(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Port for polling the light sensor data - called by rate group
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    F32 m_RawLightData;

    F32 m_IRLightData;

    F32 m_ALSLightData;

    bool m_configured = false; 

    const struct device* m_dev;
};



}  // namespace Components

#endif
