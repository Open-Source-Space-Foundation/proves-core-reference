// ======================================================================
// \title  ThermalManager.hpp
// \brief  hpp file for ThermalManager component implementation class
// ======================================================================

#ifndef Components_ThermalManager_HPP
#define Components_ThermalManager_HPP

#include "PROVESFlightControllerReference/Components/ThermalManager/ThermalManagerComponentAc.hpp"

namespace Components {

class ThermalManager final : public ThermalManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ThermalManager object
    ThermalManager(const char* const compName  //!< The component name
    );

    //! Destroy ThermalManager object
    ~ThermalManager();

  private:
    static constexpr F64 DEBOUNCE_ERROR = 3.0;  //!< Debounce error value for temperature threshold events

    bool faceTempThrottleActive[getNum_faceTempGet_OutputPorts()] = {false};
    bool battCellTempThrottleActive[getNum_battCellTempGet_OutputPorts()] = {false};

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled port for periodic temperature reading
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Helper function to log temperature threshold events
    void evaluateTemperatureThreshold(
        FwIndexType idx,       //!< The sensor index
        F64 temperature,       //!< The temperature reading
        bool& throttleActive,  //!< Whether the threshold event throttle is currently active
        Components::ThermalManager_TempSensorType sensorType  //!< The type of the temperature sensor
    );
};

}  // namespace Components

#endif
