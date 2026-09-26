// ======================================================================
// \title  PowerMonitor.hpp
// \brief  hpp file for PowerMonitor component implementation class
// ======================================================================

#ifndef Components_PowerMonitor_HPP
#define Components_PowerMonitor_HPP

#include "PROVESFlightControllerReference/Components/PowerMonitor/PowerIntegrator.hpp"
#include "PROVESFlightControllerReference/Components/PowerMonitor/PowerMonitorComponentAc.hpp"

namespace Components {

class PowerMonitor final : public PowerMonitorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PowerMonitor object
    PowerMonitor(const char* const compName  //!< The component name
    );

    //! Destroy PowerMonitor object
    ~PowerMonitor();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for RESET_TOTAL_POWER
    void RESET_TOTAL_POWER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                      U32 cmdSeq            //!< The command sequence number
                                      ) override;

    //! Handler implementation for RESET_TOTAL_GENERATION
    void RESET_TOTAL_GENERATION_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                           U32 cmdSeq            //!< The command sequence number
                                           ) override;

    //! Handler implementation for GET_TOTAL_POWER
    void GET_TOTAL_POWER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeq            //!< The command sequence number
                                    ) override;

    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Get current time in seconds
    F64 getCurrentTimeSeconds();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Integrator for accumulated power consumption and solar power generation
    PowerIntegrator m_integrator;
};

}  // namespace Components

#endif
