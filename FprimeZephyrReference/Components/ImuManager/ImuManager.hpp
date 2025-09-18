// ======================================================================
// \title  ImuManager.hpp
// \brief  hpp file for ImuManager component implementation class
// ======================================================================

#ifndef Components_ImuManager_HPP
#define Components_ImuManager_HPP

#include "FprimeZephyrReference/Components/ImuManager/ImuManagerComponentAc.hpp"

namespace Components {

class ImuManager final : public ImuManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuManager object
    ImuManager(const char* const compName);

    //! Destroy ImuManager object
    ~ImuManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
