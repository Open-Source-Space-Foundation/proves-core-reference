// ======================================================================
// \title  Updater.hpp
// \author starchmd
// \brief  hpp file for Updater component implementation class
// ======================================================================

#ifndef Zephyr_Updater_HPP
#define Zephyr_Updater_HPP

#include "FprimeZephyrReference/Components/Updater/UpdaterComponentAc.hpp"

namespace Zephyr {

class Updater final : public UpdaterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Updater object
    Updater(const char* const compName  //!< The component name
    );

    //! Destroy Updater object
    ~Updater();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command NEXT_BOOT_TEST_IMAGE
    //!
    //! Set the image for the next boot in test boot mode
    void NEXT_BOOT_TEST_IMAGE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                         U32 cmdSeq            //!< The command sequence number
                                         ) override;

    //! Handler implementation for command CONFIRM_NEXT_BOOT_IMAGE
    //!
    //! Confirm this image for future boots
    void CONFIRM_NEXT_BOOT_IMAGE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                            U32 cmdSeq            //!< The command sequence number
                                            ) override;
};

}  // namespace Zephyr

#endif
