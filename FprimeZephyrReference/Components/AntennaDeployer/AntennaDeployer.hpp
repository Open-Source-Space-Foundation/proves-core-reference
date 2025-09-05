// ======================================================================
// \title  AntennaDeployer.hpp
// \author aldjia
// \brief  hpp file for AntennaDeployer component implementation class
// ======================================================================

#ifndef Components_AntennaDeployer_HPP
#define Components_AntennaDeployer_HPP

#include "FprimeZephyrReference/Components/AntennaDeployer/AntennaDeployerComponentAc.hpp"

namespace Components {

class AntennaDeployer final : public AntennaDeployerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AntennaDeployer object
    AntennaDeployer(const char* const compName  //!< The component name
    );

    //! Destroy AntennaDeployer object
    ~AntennaDeployer();
};

}  // namespace Components

#endif
