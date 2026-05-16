// ======================================================================
// \title  SecurityRouter.hpp
// \brief  hpp file for SecurityRouter component implementation class
// ======================================================================

#ifndef Components_SecurityRouter
#define Components_SecurityRouter

#include <Fw/Types/String.hpp>
#include <atomic>
#include <cassert>

#include "PROVESFlightControllerReference/Components/SecurityRouter/Bypasser.hpp"
#include "PROVESFlightControllerReference/Components/SecurityRouter/SecurityRouterComponentAc.hpp"

namespace Components {

class SecurityRouter final : public SecurityRouterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SecurityRouter object
    SecurityRouter(const char* const compName  //!< The component name
    );

    //! Destroy SecurityRouter object
    ~SecurityRouter();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port receiving Space Packets from TcDeframer
    void dataIn_handler(FwIndexType portNum,                 //!< The port number
                        Fw::Buffer& data,                    //!< The buffer containing the packet data
                        const ComCfg::FrameContext& context  //!< The frame context associated with the packet
                        ) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of buffers sent to dataOut
    void dataReturnIn_handler(FwIndexType portNum,                 //!< The port number
                              Fw::Buffer& data,                    //!< The buffer being returned
                              const ComCfg::FrameContext& context  //!< The frame context associated with the buffer
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::atomic<U32> m_bypassPacketsCount;    //!< Count of packets that bypass authentication
};

}  // namespace Components

#endif  // Components_SecurityRouter
