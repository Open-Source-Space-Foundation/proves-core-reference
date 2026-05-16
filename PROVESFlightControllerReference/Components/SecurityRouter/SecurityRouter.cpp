// ======================================================================
// \title  SecurityRouter.cpp
// \brief  cpp file for SecurityRouter component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/SecurityRouter/SecurityRouter.hpp"

#include <Fw/Log/LogString.hpp>
#include <iomanip>
#include <utility>

#include "Bypasser.hpp"
#include "SecurityRouter.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SecurityRouter ::SecurityRouter(const char* const compName)
    : SecurityRouterComponentBase(compName),
      m_bypassPacketsCount(0) {}

SecurityRouter ::~SecurityRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SecurityRouter ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Determine whether the packet can bypass authentication based on its OpCode
    const PacketBypasser::Status bypassStatus = bypassPacket(data.getData(), data.getSize());
    if (bypassStatus == PacketBypasser::Status::OpCodeParseError) {
        this->log_WARNING_HI_ParsingFailed();
        this->dataReturnOut_out(0, data, context);
        return;
    }

    if (bypassStatus == PacketBypasser::Status::BypassAllowed) {
        // Telemeter the updated bypass packets count
        this->m_bypassPacketsCount += 1;
        this->tlmWrite_BypassPacketsCount(this->m_bypassPacketsCount);

        // Forward the packet to the output port
        this->bypassOut_out(0, data, context);
        return;
    }
    
    this->authenticateOut_out(0, data, context);
}

void SecurityRouter ::dataReturnIn_handler(FwIndexType portNum,
                                            Fw::Buffer& data,
                                            const ComCfg::FrameContext& context) {
    this->authenticateOut_out(0, data, context);
}

}  // namespace Components
