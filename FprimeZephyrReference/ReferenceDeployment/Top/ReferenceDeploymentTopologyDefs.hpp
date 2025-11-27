// ======================================================================
// \title  ReferenceDeploymentTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef REFERENCEDEPLOYMENT_REFERENCEDEPLOYMENTTOPOLOGYDEFS_HPP
#define REFERENCEDEPLOYMENT_REFERENCEDEPLOYMENTTOPOLOGYDEFS_HPP

// Subtopology PingEntries includes
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/ComCcsds/PingEntries.hpp"
#include "Svc/Subtopologies/DataProducts/PingEntries.hpp"
#include "Svc/Subtopologies/FileHandling/PingEntries.hpp"

// SubtopologyTopologyDefs includes
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/ComCcsds/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"

// ComCcsds Enum Includes
#include "Svc/Subtopologies/ComCcsds/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComCcsds/Ports_ComPacketQueueEnumAc.hpp"

// Include autocoded FPP constants
#include "FprimeZephyrReference/ReferenceDeployment/Top/FppConstantsAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

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
namespace ReferenceDeployment_rateGroup10Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_rateGroup1Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

// Definitions are placed within a namespace named after the deployment
namespace ReferenceDeployment {

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires an object that carries state with the name `ReferenceDeployment::TopologyState`. Only
 * the type definition is required by the autocoder and the contents of this object are otherwise opaque to the
 * autocoder. The contents are entirely up to the definition of the project. This deployment uses subtopologies.
 */
struct TopologyState {
    const device* uartDevice;  //!< UART device path for communication
    const device* loraDevice;  //!< LoRa device path for communication

    U32 baudRate;                                 //!< Baud rate for UART communication
    CdhCore::SubtopologyState cdhCore;            //!< Subtopology state for CdhCore
    ComCcsds::SubtopologyState comCcsds;          //!< Subtopology state for ComCcsds
    FileHandling::SubtopologyState fileHandling;  //!< Subtopology state for FileHandling
    const device* ina219SysDevice;                //!< device path for battery board ina219
    const device* ina219SolDevice;                //!< device path for solar panel ina219
    const device* lsm6dsoDevice;                  //!< LSM6DSO device path for accelerometer/gyroscope
    const device* lis2mdlDevice;                  //!< LIS2MDL device path for magnetometer
    const device* rtcDevice;                      //!< RTC device path
    const device* mcp23017;                       //!< MCP23017 GPIO expander device path

    // Face devices
    // - Temperature sensors
    const device* face0TempDevice;      //!< TMP112 device for cube face 0
    const device* face1TempDevice;      //!< TMP112 device for cube face 1
    const device* face2TempDevice;      //!< TMP112 device for cube face 2
    const device* face3TempDevice;      //!< TMP112 device for cube face 3
    const device* face5TempDevice;      //!< TMP112 device for cube face 5
    const device* battCell1TempDevice;  //!< TMP112 device for battery cell 1
    const device* battCell2TempDevice;  //!< TMP112 device for battery cell 2
    const device* battCell3TempDevice;  //!< TMP112 device for battery cell 3
    const device* battCell4TempDevice;  //!< TMP112 device for battery cell 4
};

namespace PingEntries = ::PingEntries;
}  // namespace ReferenceDeployment
#endif
