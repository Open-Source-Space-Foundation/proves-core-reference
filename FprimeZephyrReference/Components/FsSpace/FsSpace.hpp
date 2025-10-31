// ======================================================================
// \title  FsSpace.hpp
// \author starchmd
// \brief  hpp file for FsSpace component implementation class
// ======================================================================

#ifndef Components_FsSpace_HPP
#define Components_FsSpace_HPP

#include "FprimeZephyrReference/Components/FsSpace/FsSpaceComponentAc.hpp"

namespace Components {

class FsSpace final : public FsSpaceComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FsSpace object
    FsSpace(const char* const compName  //!< The component name
    );

    //! Destroy FsSpace object
    ~FsSpace();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
