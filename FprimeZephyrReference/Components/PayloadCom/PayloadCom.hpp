// ======================================================================
// \title  PayloadCom.hpp
// \author robertpendergrast, moisesmata
// \brief  hpp file for PayloadCom component implementation class
// ======================================================================

#ifndef FprimeZephyrReference_PayloadCom_HPP
#define FprimeZephyrReference_PayloadCom_HPP

#include "FprimeZephyrReference/Components/PayloadCom/PayloadComComponentAc.hpp"

namespace Components {

class PayloadCom final : public PayloadComComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PayloadCom object
    PayloadCom(const char* const compName  //!< The component name
    );

    //! Destroy PayloadCom object
    ~PayloadCom();


  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for uartDataIn port
    //! Forwards data to CameraHandler and sends ACK
    void uartDataIn_handler(FwIndexType portNum,  //!< The port number
                            Fw::Buffer& buffer,
                            const Drv::ByteStreamStatus& status);

    //! Handler implementation for commandIn port
    //! Forwards command to UART
    void commandIn_handler(FwIndexType portNum,  //!< The port number
                           Fw::Buffer& buffer,
                           const Drv::ByteStreamStatus& status);

    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Send acknowledgment over UART
    void sendAck();
};

}  // namespace Components

#endif
