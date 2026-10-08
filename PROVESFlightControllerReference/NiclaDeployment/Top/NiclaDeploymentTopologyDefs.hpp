// ======================================================================
// \title  NiclaDeploymentTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef NICLADEPLOYMENT_NICLADEPLOYMENTTOPOLOGYDEFS_HPP
#define NICLADEPLOYMENT_NICLADEPLOYMENTTOPOLOGYDEFS_HPP

// Subtopology PingEntries includes
#include "PROVESFlightControllerReference/ComCcsdsNicla/PingEntries.hpp"

// SubtopologyTopologyDefs includes
#include "PROVESFlightControllerReference/ComCcsdsNicla/SubtopologyTopologyDefs.hpp"

// ComCcsdsNicla Enum Includes
#include "PROVESFlightControllerReference/ComCcsdsNicla/Ports_ComBufferQueueEnumAc.hpp"
#include "PROVESFlightControllerReference/ComCcsdsNicla/Ports_ComPacketQueueEnumAc.hpp"

// Include autocoded FPP constants
#include "PROVESFlightControllerReference/NiclaDeployment/Top/FppConstantsAc.hpp"
#include <zephyr/drivers/uart.h>

/**
 * \brief required ping constants
 *
 * The topology autocoder requires a WARN and FATAL constant definition for each component that supports the health-ping
 * interface. These are expressed as enum constants placed in a namespace named for the component instance. These
 * are all placed in the PingEntries namespace.
 *
 * Each constant specifies how many missed pings are allowed before a WARNING_HI/FATAL event is triggered. In the
 * following example, the health component will emit a WARNING_HI event if the component instance cmdDisp does not
 * respond for 3 pings and will FATAL if responses are not received after a total of 5 pings.
 *
 * ```c++
 * namespace PingEntries {
 * namespace cmdDisp {
 *     enum { WARN = 3, FATAL = 5 };
 * }
 * }
 * ```
 */
namespace PingEntries {
namespace NiclaDeployment_rateGroup10Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace NiclaDeployment_rateGroup1Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace NiclaDeployment_cmdDisp {
enum { WARN = 3, FATAL = 5 };
}
namespace NiclaDeployment_events {
enum { WARN = 3, FATAL = 5 };
}
namespace NiclaDeployment_tlmSend {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

// Definitions are placed within a namespace named after the deployment
namespace NiclaDeployment {

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires an object that carries state with the name `NiclaDeployment::TopologyState`. Only
 * the type definition is required by the autocoder and the contents of this object are otherwise opaque to the
 * autocoder. The contents are entirely up to the definition of the project. This deployment uses subtopologies.
 */
struct TopologyState {
    const device* uartDevice;                       //!< UART device path for communication
    U32 baudRate;                                   //!< Baud rate for UART communication
    ComCcsdsNicla::SubtopologyState comCcsdsNicla;  //!< Subtopology state for ComCcsdsNicla
};

namespace PingEntries = ::PingEntries;
}  // namespace NiclaDeployment
#endif
