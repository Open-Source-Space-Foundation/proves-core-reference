// ======================================================================
// \title  StackMonitor.hpp
// \brief  hpp file for StackMonitor component implementation class
// ======================================================================

#ifndef Components_StackMonitor_HPP
#define Components_StackMonitor_HPP

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorCore.hpp"

namespace Components {

class StackMonitor final : public StackMonitorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StackMonitor object
    StackMonitor(const char* const compName  //!< The component name
    );

    //! Destroy StackMonitor object
    ~StackMonitor();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Warn when a thread's free stack drops below this percent of its own size.
    static constexpr std::uint32_t WARN_THRESHOLD_PERCENT = 20;

    //! Pure-logic core: turns a snapshot of per-thread stack usage into a
    //! summary and warn/clear decisions. Host-testable in isolation; see
    //! test/unit-tests/test_StackMonitor_Core.cpp.
    StackMonitorCore m_core;

    //! Per-tick sample and result storage. Fixed-size and kept as member
    //! state (not run_handler locals): together they are a few KB, which
    //! would not be comfortable on the 4 KB rate-group thread stack, and
    //! keeping them here guarantees zero heap allocation after boot.
    ThreadStackSampleSet m_samples;
    StackMonitorTickResult m_result;
};

}  // namespace Components

#endif
