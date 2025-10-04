// ======================================================================
// \title  BootloaderTrigger.hpp
// \author aychar
// \brief  hpp file for BootloaderTrigger component implementation class
// ======================================================================

#ifndef Components_BootloaderTrigger_HPP
#define Components_BootloaderTrigger_HPP

#include "FprimeZephyrReference/Components/BootloaderTrigger/BootloaderTriggerComponentAc.hpp"

namespace Components {

class BootloaderTrigger final : public BootloaderTriggerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BootloaderTrigger object
    BootloaderTrigger(const char* const compName  //!< The component name
    );

    //! Destroy BootloaderTrigger object
    ~BootloaderTrigger();

  private:
    void TRIGGER_BOOTLOADER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;
};

}  // namespace Components

#endif
