// ======================================================================
// \title  ComDelay.hpp
// \author starchmd
// \brief  hpp file for ComDelay component implementation class
// ======================================================================

#ifndef Components_ComDelay_HPP
#define Components_ComDelay_HPP

#include <atomic>
#include "FprimeZephyrReference/Components/ComDelay/ComDelayComponentAc.hpp"

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
    //! Count of incoming run ticks
    U8 m_tick_count;
    //! Stores if the last status is currently valid
    std::atomic<bool> m_last_status_valid;
    //! Stores the last status
    Fw::Success m_last_status;
};

}  // namespace Components

#endif
