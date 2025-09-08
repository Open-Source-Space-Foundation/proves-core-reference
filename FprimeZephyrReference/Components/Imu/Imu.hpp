// ======================================================================
// \title  Imu.hpp
// \author aychar
// \brief  hpp file for Imu component implementation class
// ======================================================================

#ifndef Components_Imu_HPP
#define Components_Imu_HPP

#include "FprimeZephyrReference/Components/Imu/ImuComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Components {

class Imu final : public ImuComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Imu object
    Imu(const char* const compName  //!< The component name
    );

    //! Destroy Imu object
    ~Imu();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for TODO
    //!
    //! TODO
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    static float out_ev(struct sensor_value* val);
    const struct device* lis2mdl;
    const struct device* lsm6dso;
    struct sensor_value odr;
    
};

}  // namespace Components

#endif
