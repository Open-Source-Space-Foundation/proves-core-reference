// ======================================================================
// \title  ReferenceDeploymentTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef REFERENCEDEPLOYMENT_REFERENCEDEPLOYMENTTOPOLOGYDEFS_HPP
#define REFERENCEDEPLOYMENT_REFERENCEDEPLOYMENTTOPOLOGYDEFS_HPP

#include <map>
#include <string>

// Subtopology PingEntries includes
#include "FprimeZephyrReference/ComCcsdsLora/PingEntries.hpp"
// #include "FprimeZephyrReference/ComCcsdsSband/PingEntries.hpp"
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/DataProducts/PingEntries.hpp"
// Replaced with override section below
// #include "Svc/Subtopologies/FileHandling/PingEntries.hpp"

// SubtopologyTopologyDefs includes
#include "FprimeZephyrReference/ComCcsdsLora/SubtopologyTopologyDefs.hpp"
// #include "FprimeZephyrReference/ComCcsdsSband/SubtopologyTopologyDefs.hpp"
#include "FprimeZephyrReference/ComCcsdsUart/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"

// ComCcsds Enum Includes (for ComCcsdsLora)
#include "Svc/Subtopologies/ComCcsds/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComCcsds/Ports_ComPacketQueueEnumAc.hpp"
// ComCcsdsUart Enum Includes
#include "FprimeZephyrReference/ComCcsdsUart/Ports_ComBufferQueueEnumAc.hpp"
#include "FprimeZephyrReference/ComCcsdsUart/Ports_ComPacketQueueEnumAc.hpp"

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

// Override section for FileHandling PingEntries
namespace PingEntries {
namespace FileHandling_fileDownlink {
enum { WARN = 3, FATAL = 5 };
}
namespace FileHandling_fileManager {
enum { WARN = 30, FATAL = 60 };
}
namespace FileHandling_fileUplink {
enum { WARN = 3, FATAL = 5 };
}
namespace FileHandling_prmDb {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

namespace PingEntries {
namespace ReferenceDeployment_rateGroup50Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_rateGroup10Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_rateGroup1Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_rateGroup1_6Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_rateGroup1_10Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_payloadSeq {
enum { WARN = 3, FATAL = 5 };
}
namespace ReferenceDeployment_safeModeSeq {
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
    const device* uartDevice;                     //!< UART device path for communication
    const device* spi0Device;                     //!< Spi device path for s-band LoRa module
    const device* loraDevice;                     //!< LoRa device path for communication
    ComCcsdsLora::SubtopologyState comCcsdsLora;  //!< Subtopology state for ComCcsdsLora
    // ComCcsdsSband::SubtopologyState comCcsdsSband;  //!< Subtopology state for ComCcsdsSband
    U32 baudRate;                       //!< Baud rate for UART communication
    CdhCore::SubtopologyState cdhCore;  //!< Subtopology state for CdhCore
    // ComCcsdsUart::SubtopologyState comCcsdsUart;    //!< Subtopology state for ComCcsds
    const device* peripheralUart;
    U32 peripheralBaudRate;
    const device* peripheralUart2;
    U32 peripheralBaudRate2;
    FileHandling::SubtopologyState fileHandling;  //!< Subtopology state for FileHandling
    const device* ina219SysDevice;                //!< device path for battery board ina219
    const device* ina219SolDevice;                //!< device path for solar panel ina219
    const device* lsm6dsoDevice;                  //!< LSM6DSO device path for accelerometer/gyroscope
    const device* lis2mdlDevice;                  //!< LIS2MDL device path for magnetometer
    const device* rtcDevice;                      //!< RTC device path
    const device* tca9548aDevice;                 //!< TCA9548A I2C multiplexer device
    const device* muxChannel0Device;              //!< Multiplexer channel 0 device
    const device* muxChannel1Device;              //!< Multiplexer channel 1 device
    const device* muxChannel2Device;              //!< Multiplexer channel 2 device
    const device* muxChannel3Device;              //!< Multiplexer channel 3 device
    const device* muxChannel4Device;              //!< Multiplexer channel 4 device
    const device* muxChannel5Device;              //!< Multiplexer channel 5 device
    const device* muxChannel6Device;              //!< Multiplexer channel 5 device
    const device* muxChannel7Device;              //!< Multiplexer channel 7 device
    int storagePartitionId;                       //!< Storage partition ID

    // Face devices
    //! Temperature sensors
    const device* face0TempDevice;      //!< TMP112 device for cube face 0
    const device* face1TempDevice;      //!< TMP112 device for cube face 1
    const device* face2TempDevice;      //!< TMP112 device for cube face 2
    const device* face3TempDevice;      //!< TMP112 device for cube face 3
    const device* face5TempDevice;      //!< TMP112 device for cube face 5
    const device* battCell1TempDevice;  //!< TMP112 device for battery cell 1
    const device* battCell2TempDevice;  //!< TMP112 device for battery cell 2
    const device* battCell3TempDevice;  //!< TMP112 device for battery cell 3
    const device* battCell4TempDevice;  //!< TMP112 device for battery cell 4
    //! Light sensors
    const device* face0LightDevice;  //!< Light sensor device for cube face 0
    const device* face1LightDevice;  //!< Light sensor device for cube face 1
    const device* face2LightDevice;  //!< Light sensor device for cube face 2
    const device* face3LightDevice;  //!< Light sensor device for cube face 3
    const device* face5LightDevice;  //!< Light sensor device for cube face 5
    const device* face6LightDevice;  //!< Light sensor device for cube face 6
    const device* face7LightDevice;  //!< Light sensor device for cube face 7
    //! Magnetorquers
    const device* face0drv2605Device;
    const device* face1drv2605Device;
    const device* face2drv2605Device;
    const device* face3drv2605Device;
    const device* face5drv2605Device;
};

namespace PingEntries = ::PingEntries;
}  // namespace ReferenceDeployment
#endif
