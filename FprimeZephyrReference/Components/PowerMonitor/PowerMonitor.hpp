// ======================================================================
// \title  PowerMonitor.hpp
// \author nate
// \brief  hpp file for PowerMonitor component implementation class
// ======================================================================

#ifndef Components_PowerMonitor_HPP
#define Components_PowerMonitor_HPP

#include "FprimeZephyrReference/Components/PowerMonitor/PowerMonitorComponentAc.hpp"

namespace Components {

class PowerMonitor final : public PowerMonitorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PowerMonitor object
    PowerMonitor(const char* const compName  //!< The component name
    );

    //! Destroy PowerMonitor object
    ~PowerMonitor();
};

}  // namespace Components

#endif
