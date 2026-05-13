// ======================================================================
// \title  ADCS.hpp
// \brief  hpp file for ADCS component implementation class
// ======================================================================

#ifndef Components_ADCS_HPP
#define Components_ADCS_HPP

#include "PROVESFlightControllerReference/Components/ADCS/ADCSComponentAc.hpp"

namespace Components {

class ADCS final : public ADCSComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ADCS object
    ADCS(const char* const compName  //!< The component name
    );

    //! Destroy ADCS object
    ~ADCS();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled port for periodic temperature reading
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
