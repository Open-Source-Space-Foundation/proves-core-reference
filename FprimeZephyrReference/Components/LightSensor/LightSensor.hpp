// ======================================================================
// \title  LightSensor.hpp
// \author robertpendergrast
// \brief  hpp file for LightSensor component implementation class
// ======================================================================

#ifndef Components_LightSensor_HPP
#define Components_LightSensor_HPP

#include "FprimeZephyrReference/Components/LightSensor/LightSensorComponentAc.hpp"
#include <zephyr/device.h>

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

    // Configures the light sensor
    void configure(const struct device *dev );

    void read_sensor_data();

    const struct device *m_dev;  
    bool m_configured = false;

    F32 light;
    F32 als;
    F32 ir;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
