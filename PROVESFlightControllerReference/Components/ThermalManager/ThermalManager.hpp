// ======================================================================
// \title  ThermalManager.hpp
// \brief  hpp file for ThermalManager component implementation class
// ======================================================================

#ifndef Components_ThermalManager_HPP
#define Components_ThermalManager_HPP

#include "PROVESFlightControllerReference/Components/ThermalManager/ThermalManagerComponentAc.hpp"
#include <vector>

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

    std::vector<bool> m_faceTempBelowActive;
    std::vector<bool> m_faceTempAboveActive;
    std::vector<bool> m_battCellTempBelowActive;
    std::vector<bool> m_battCellTempAboveActive;

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
