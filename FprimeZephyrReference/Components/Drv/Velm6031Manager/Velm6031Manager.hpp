// ======================================================================
// \title  Velm6031Manager.hpp
// \brief  hpp file for Velm6031Manager component implementation class
// ======================================================================

#ifndef Components_Velm6031Manager_HPP
#define Components_Velm6031Manager_HPP

#include <stdio.h>

#include "FprimeZephyrReference/Components/Velm6031Manager/Velm6031ManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/veml6031.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>

namespace Components {

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

    void ReadData();

    void configure(const struct device* dev);

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

    void init_handler(FwIndexType portNum);

    F32 m_RawLightData;

    F32 m_IRLightData;

    F32 m_ALSLightData;

    bool m_configured = false;

    bool m_attributes_set = false;

    bool m_device_init = false;

    const struct device* m_dev;
};

}  // namespace Components

#endif
