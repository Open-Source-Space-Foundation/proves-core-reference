// ======================================================================
// \title  ComDelay.hpp
// \author starchmd
// \brief  hpp file for ComDelay component implementation class
// ======================================================================

#ifndef Components_ComDelay_HPP
#define Components_ComDelay_HPP

#include <atomic>

#include "PROVESFlightControllerReference/Components/ComDelay/ComDelayComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/ComDelay/TickDivider.hpp"

namespace Components {

class ComDelay final : public ComDelayComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ComDelay object
    ComDelay(const char* const compName  //!< The component name
    );

    //! Destroy ComDelay object
    ~ComDelay();

  private:
    void parameterUpdated(FwPrmIdType id  //!< The parameter ID
                          ) override;

    //! Ignore parameter loads; parameterUpdated reports changes, and loading at boot is not a change
    void parameterLoaded(FwPrmIdType id, Fw::ParamValid valid) override;

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comStatusIn
    //!
    //! Input comStatus from radio component
    void comStatusIn_handler(FwIndexType portNum,    //!< The port number
                             Fw::Success& condition  //!< Condition success/failure
                             ) override;

    //! Handler implementation for run
    //!
    //! Rate schedule port used to trigger radio transmission
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    //! Divides incoming run ticks down to com status releases
    TickDivider m_divider;
    //! Stores if the last status is currently valid
    std::atomic<bool> m_last_status_valid;
    //! Stores the last status
    Fw::Success m_last_status;
};

}  // namespace Components

#endif
