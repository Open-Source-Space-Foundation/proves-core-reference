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
    static constexpr U32 NUM_FACE_TEMP_SENSORS = 5;
    static constexpr U32 NUM_BATT_CELL_TEMP_SENSORS = 4;

    bool faceTempBelowThrottleActive[NUM_FACE_TEMP_SENSORS] = {false};
    bool faceTempAboveThrottleActive[NUM_FACE_TEMP_SENSORS] = {false};
    bool battCellTempBelowThrottleActive[NUM_BATT_CELL_TEMP_SENSORS] = {false};
    bool battCellTempAboveThrottleActive[NUM_BATT_CELL_TEMP_SENSORS] = {false};

    using LogFn = void (ThermalManager::*)(U32, F32) const;

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
        FwIndexType idx,            //!< The sensor index
        F64 temperature,            //!< The temperature reading
        F64 lowerThreshold,         //!< The lower temperature threshold
        F64 upperThreshold,         //!< The upper temperature threshold
        bool& belowThrottleActive,  //!< Whether the below threshold event throttle is currently active
        bool& aboveThrottleActive,  //!< Whether the above threshold event throttle is currently active
        LogFn logBelow,             //!< Log function for below threshold event
        LogFn logAbove);            //!< Log function for above threshold event
};

}  // namespace Components

#endif
