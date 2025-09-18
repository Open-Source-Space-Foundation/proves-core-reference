// ======================================================================
// \title  Rv3028Driver.hpp
// \brief  hpp file for Rv3028Driver component implementation class
// ======================================================================

#ifndef Components_Rv3028Driver_HPP
#define Components_Rv3028Driver_HPP

#include "FprimeZephyrReference/Components/Drv/Rv3028Driver/Rv3028DriverComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/timeutil.h>

namespace Drv {

class Rv3028Driver final : public Rv3028DriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Rv3028Driver object
    Rv3028Driver(const char* const compName  //!< The component name
    );

    //! Destroy Rv3028Driver object
    ~Rv3028Driver();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for timeGetPort
    //!
    //! Port to retrieve time
    void timeGetPort_handler(FwIndexType portNum,  //!< The port number
                             Fw::Time& time        //!< Reference to Time object
                             ) override;

    //! Handler implementation for timeRead
    //!
    //! timeRead port to get the time from the RTC
    //! Requirement Rv3028Driver-002
    U32 timeRead_handler(FwIndexType portNum  //!< The port number
                         ) override;

    //! Handler implementation for timeSet
    //!
    //! timeSet port to set the time on the RTC
    //! Requirement Rv3028Driver-001
    void timeSet_handler(FwIndexType portNum,  //!< The port number
                         const Drv::TimeData& time) override;

    //! Zephyr device stores the initialized RV2038 sensor
    const struct device* rv3028;
};

}  // namespace Drv

#endif
