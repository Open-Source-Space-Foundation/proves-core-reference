// ======================================================================
// \title  INA219Manager.hpp
// \author ncc-michael
// \brief  hpp file for INA219Manager component implementation class
// ======================================================================

#ifndef Components_INA219Manager_HPP
#define Components_INA219Manager_HPP

#include "FprimeZephyrReference/Components/INA219Manager/INA219ManagerComponentAc.hpp"

namespace Components {

class INA219Manager final : public INA219ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct INA219Manager object
    INA219Manager(const char* const compName  //!< The component name
    );

    //! Destroy INA219Manager object
    ~INA219Manager();

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
