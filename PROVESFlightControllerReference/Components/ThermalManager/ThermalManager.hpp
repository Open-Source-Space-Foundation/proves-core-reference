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

    bool m_faceTempBelowActive[NUM_FACE_TEMP_SENSORS] = {false};
    bool m_faceTempAboveActive[NUM_FACE_TEMP_SENSORS] = {false};
    bool m_battCellTempBelowActive[NUM_BATT_CELL_TEMP_SENSORS] = {false};
    bool m_battCellTempAboveActive[NUM_BATT_CELL_TEMP_SENSORS] = {false};

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled port for periodic temperature reading
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
