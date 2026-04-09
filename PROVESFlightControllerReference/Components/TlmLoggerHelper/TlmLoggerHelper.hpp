// ======================================================================
// \title  TlmLoggerHelper.hpp
// \author aaron
// \brief  hpp file for TlmLoggerHelper component implementation class
// ======================================================================

#ifndef Components_TlmLoggerHelper_HPP
#define Components_TlmLoggerHelper_HPP

#include "PROVESFlightControllerReference/Components/TlmLoggerHelper/TlmLoggerHelperComponentAc.hpp"

namespace Components {

class TlmLoggerHelper final : public TlmLoggerHelperComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TlmLoggerHelper object
    TlmLoggerHelper(const char* const compName  //!< The component name
    );

    //! Destroy TlmLoggerHelper object
    ~TlmLoggerHelper();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comIn
    void comIn_handler(FwIndexType portNum,  //!< The port number
                       Fw::ComBuffer& data,  //!< Buffer containing packet data
                       U32 context           //!< Call context value; meaning chosen by user
                       ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command CONNECT_TLM_LOGGER
    void CONNECT_TLM_LOGGER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;
    bool m_connected = false;
};


}  // namespace Components

#endif
