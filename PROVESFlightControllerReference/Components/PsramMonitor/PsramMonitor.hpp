// ======================================================================
// \title  PsramMonitor.hpp
// \brief  F' passive component that monitors the RP2350 QMI PSRAM.
//         Issue #285, slice S6.
// ======================================================================

#ifndef Components_PsramMonitor_HPP
#define Components_PsramMonitor_HPP

#include "PROVESFlightControllerReference/Components/PsramMonitor/PsramMonitorComponentAc.hpp"

namespace Components {

class PsramMonitor final : public PsramMonitorComponentBase {
  public:
    // Maximum bytes tested per PSRAM_SELF_TEST command invocation.
    // Bounded to prevent blocking the command dispatcher for > ~10 ms.
    static constexpr U32 SELF_TEST_LENGTH_CAP = 256U * 1024U;  // 256 KB

    //! Construct PsramMonitor object
    explicit PsramMonitor(const char* const compName);

    //! Destroy PsramMonitor object
    ~PsramMonitor();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler for the 1 Hz rate-group run port.
    //! Emits PsramSize and PsramStatus telemetry (cheap reads only).
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler for PSRAM_SELF_TEST command.
    void PSRAM_SELF_TEST_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeqNum,        //!< The command sequence number
                                    U32 start_offset,     //!< Byte offset from PSRAM base
                                    U32 length            //!< Number of bytes to test
                                    ) override;
};

}  // namespace Components

#endif
