// ======================================================================
// \title  AmateurRadio.hpp
// \author t38talon
// \brief  hpp file for AmateurRadio component implementation class
// ======================================================================

#ifndef Components_AmateurRadio_HPP
#define Components_AmateurRadio_HPP

#include "FprimeZephyrReference/Components/AmateurRadio/AmateurRadioComponentAc.hpp"

namespace Components {

class AmateurRadio final : public AmateurRadioComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AmateurRadio object
    AmateurRadio(const char* const compName  //!< The component name
    );

    //! Destroy AmateurRadio object
    ~AmateurRadio();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command Repeat_Name
    //!
    //! The satelie will repeat back the radio name
    void Repeat_Name_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                const Fw::CmdStringArg& radio_name) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Counter for number of tags received
    U32 m_count_names;
};

}  // namespace Components

#endif
