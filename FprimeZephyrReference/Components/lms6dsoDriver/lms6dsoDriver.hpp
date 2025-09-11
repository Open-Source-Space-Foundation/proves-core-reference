// ======================================================================
// \title  lms6dsoDriver.hpp
// \author aaron
// \brief  hpp file for lms6dsoDriver component implementation class
// ======================================================================

#ifndef Components_lms6dsoDriver_HPP
#define Components_lms6dsoDriver_HPP

#include "FprimeZephyrReference/Components/lms6dsoDriver/lms6dsoDriverComponentAc.hpp"

namespace Components {

class lms6dsoDriver final : public lms6dsoDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct lms6dsoDriver object
    lms6dsoDriver(const char* const compName  //!< The component name
    );

    //! Destroy lms6dsoDriver object
    ~lms6dsoDriver();
};

}  // namespace Components

#endif
