// ======================================================================
// \title  NicolaVisionHandler.hpp
// \author wisaac
// \brief  hpp file for NicolaVisionHandler component implementation class
// ======================================================================

#ifndef Components_NicolaVisionHandler_HPP
#define Components_NicolaVisionHandler_HPP

#include "FprimeZephyrReference/Components/NicolaVisionHandler/NicolaVisionHandlerComponentAc.hpp"

namespace Components {

class NicolaVisionHandler final : public NicolaVisionHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct NicolaVisionHandler object
    NicolaVisionHandler(const char* const compName  //!< The component name
    );

    //! Destroy NicolaVisionHandler object
    ~NicolaVisionHandler();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for in_port
    Fw::Buffer pic_in_buffer;
    const char snap_cmd[4] = {'s', 'n', 'a', 'p'};
    const char magic_bytes[4] = {'s', 'i', 'z', 'e'};
    FwSizeType image_len = 0;
    bool found_magic = false;
    FwSizeType magic_pos = 0;
    FwSizeType image_received = 0;
    FwSizeType i = 0;
    FwSizeType bytes_from_here = 0;
    FwSizeType to_copy = 0;
    FwSizeType szremain = 0;
    void in_port_handler(FwIndexType portNum,  //!< The port number
                         Fw::Buffer& buffer,
                         const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TakePicture
    void TakePicture_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;
};

}  // namespace Components

#endif
