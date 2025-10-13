// ======================================================================
// \title  Authenticate.hpp
// \author t38talon
// \brief  hpp file for Authenticate component implementation class
// ======================================================================

#ifndef Components_Authenticate_HPP
#define Components_Authenticate_HPP

#include "FprimeZephyrReference/Components/Authenticate/AuthenticateComponentAc.hpp"

namespace Components {

class Authenticate final : public AuthenticateComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Authenticate object
    Authenticate(const char* const compName  //!< The component name
    );

    //! Destroy Authenticate object
    ~Authenticate();
};

}  // namespace Components

#endif
