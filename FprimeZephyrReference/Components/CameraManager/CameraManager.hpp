// ======================================================================
// \title  CameraManager.hpp
// \author robertpendergrast
// \brief  hpp file for CameraManager component implementation class
// ======================================================================

#ifndef FprimeZephyrReference_CameraManager_HPP
#define FprimeZephyrReference_CameraManager_HPP

#include "FprimeZephyrReference/Components/CameraManager/CameraManagerComponentAc.hpp"

namespace Components {

class CameraManager final : public CameraManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CameraManager object
    CameraManager(const char* const compName  //!< The component name
    );

    //! Destroy CameraManager object
    ~CameraManager();

    U8 snapArray[5] = {'s', 'n', 'a', 'p', '\n'};

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TAKE_IMAGE
    //!
    //! TODO
    void TAKE_IMAGE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;
};

}  // namespace FprimeZephyrReference

#endif
