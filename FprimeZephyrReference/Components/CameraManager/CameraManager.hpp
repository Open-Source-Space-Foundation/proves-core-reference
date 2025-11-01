// ======================================================================
// \title  CameraManager.hpp
// \author robertpendergrast
// \brief  hpp file for CameraManager component implementation class
// ======================================================================

#ifndef FprimeZephyrReference_CameraManager_HPP
#define FprimeZephyrReference_CameraManager_HPP

#include "FprimeZephyrReference/Components/CameraManager/CameraManagerComponentAc.hpp"

namespace FprimeZephyrReference {

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

    const char snap_cmd[4] = {'s', 'n', 'a', 'p'};

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
