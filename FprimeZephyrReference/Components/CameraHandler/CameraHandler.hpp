// ======================================================================
// \title  CameraHandler.hpp
// \author moises
// \brief  hpp file for CameraHandler component implementation class
// ======================================================================

#ifndef Components_CameraHandler_HPP
#define Components_CameraHandler_HPP

#include "FprimeZephyrReference/Components/CameraHandler/CameraHandlerComponentAc.hpp"

namespace Components {

class CameraHandler final : public CameraHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CameraHandler object
    CameraHandler(const char* const compName  //!< The component name
    );

    //! Destroy CameraHandler object
    ~CameraHandler();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TODO
    //!
    //! TODO
    void TODO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;
};

}  // namespace Components

#endif
