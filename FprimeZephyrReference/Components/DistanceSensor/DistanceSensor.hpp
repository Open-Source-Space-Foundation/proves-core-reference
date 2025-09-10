// ======================================================================
// \title  DistanceSensor.hpp
// \author aldjia
// \brief  hpp file for DistanceSensor component implementation class
// ======================================================================

#ifndef Components_DistanceSensor_HPP
#define Components_DistanceSensor_HPP

#include "FprimeZephyrReference/Components/DistanceSensor/DistanceSensorComponentAc.hpp"

namespace Components {

class DistanceSensor final : public DistanceSensorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DistanceSensor object
    DistanceSensor(const char* const compName  //!< The component name
    );

    //! Destroy DistanceSensor object
    ~DistanceSensor();
};

}  // namespace Components

#endif
