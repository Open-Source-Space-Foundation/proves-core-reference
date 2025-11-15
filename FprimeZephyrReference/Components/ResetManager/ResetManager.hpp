// ======================================================================
// \title  ResetManager.hpp
// \author nate
// \brief  hpp file for ResetManager component implementation class
// ======================================================================

#ifndef Components_ResetManager_HPP
#define Components_ResetManager_HPP

#include "FprimeZephyrReference/Components/ResetManager/ResetManagerComponentAc.hpp"

namespace Components {

class ResetManager final : public ResetManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ResetManager object
    ResetManager(const char* const compName  //!< The component name
    );

    //! Destroy ResetManager object
    ~ResetManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for coldReset
    //!
    //! Port to invoke a cold reset
    void coldReset_handler(FwIndexType portNum  //!< The port number
                           ) override;

    //! Handler implementation for warmReset
    //!
    //! Port to invoke a warm reset
    void warmReset_handler(FwIndexType portNum  //!< The port number
                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command COLD_RESET
    //!
    //! Command to initiate a cold reset
    void COLD_RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command WARM_RESET
    //!
    //! Command to initiate a warm reset
    void WARM_RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command RESET_RADIO
    //!
    //! Command to reset the radio module
    void RESET_RADIO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Handler for cold reset
    void handleColdReset();

    //! Handler for warm reset
    void handleWarmReset();

    //! Handler for radio reset
    void handleRadioReset();
};

}  // namespace Components

#endif
