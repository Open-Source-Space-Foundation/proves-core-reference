// ======================================================================
// \title  TcaManager.hpp
// \author moisesmata
// \brief  hpp file for TcaManager component implementation class
// ======================================================================

#ifndef Components_TcaManager_HPP
#define Components_TcaManager_HPP

#include "PROVESFlightControllerReference/Components/TcaManager/TcaManagerComponentAc.hpp"
// Need to explicitly include because Fw.On is no longer a tlm channel I believe
#include <atomic>

#include "Fw/Types/OnEnumAc.hpp"

namespace Components {

class TcaManager : public TcaManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TcaManager object
    TcaManager(const char* const compName  //!< The component name
    );

    //! Destroy TcaManager object
    ~TcaManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for tcaError
    //!
    //! Port to reset the mux when a TCA error is detected
    void tcaError_handler(FwIndexType portNum  //!< The port number
                          ) override;
};

}  // namespace Components

#endif
