// ======================================================================
// \title  LightSensorManager.hpp
// \author shirleydeng
// \brief  hpp file for LightSensorManager component implementation class
// ======================================================================

#ifndef Components_LightSensorManager_HPP
#define Components_LightSensorManager_HPP

#include "FprimeZephyrReference/Components/LightSensorManager/LightSensorManagerComponentAc.hpp"

namespace Components {

class LightSensorManager final : public LightSensorManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct LightSensorManager object
    LightSensorManager(const char* const compName  //!< The component name
    );

    //! Destroy LightSensorManager object
    ~LightSensorManager();
};

}  // namespace Components

#endif
