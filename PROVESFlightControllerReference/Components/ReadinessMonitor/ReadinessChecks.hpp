// ======================================================================
// \title  ReadinessChecks.hpp
// \brief  Pure verdict functions for ReadinessMonitor. No ports, no OS calls.
// ======================================================================

#ifndef Components_ReadinessChecks_HPP
#define Components_ReadinessChecks_HPP

#include <Fw/FPrimeBasicTypes.hpp>

#include "PROVESFlightControllerReference/Components/ReadinessMonitor/CheckResultEnumAc.hpp"

namespace Components {
namespace Readiness {

struct Verdict {
    CheckResult::T result;
    I32 detail;
};

//! Detail when the source channel or parameter has never produced a value
constexpr I32 DETAIL_NO_DATA = -1;

//! Counts seconds a counter has not moved. A missing reading counts as no movement.
class StallTimer {
  public:
    //! \return seconds since the value last changed
    U32 update(bool valid, U64 value, U32 elapsedS);

  private:
    bool m_seen = false;
    U64 m_last = 0;
    U32 m_stalledS = 0;
};

//! Counts seconds a condition has held without a break
U32 accumulate(bool condition, U32 priorS, U32 elapsedS);

enum FsRootDetail : I32 {
    ROOT_UNREADABLE = 1,
    ROOT_GARBAGE_NAME = 2,
    ROOT_NO_BOOT_COUNT = 4,
};

struct RootListing {
    bool readable;
    U32 garbageNames;
    bool bootCountPresent;
};

//! An entry name a healthy FAT root never holds: empty, or containing control or non-ASCII bytes
bool isGarbageName(const char* name);

Verdict fsRoot(const RootListing& listing);

//! detail is free space in KiB
Verdict fsSpace(bool valid, FwSizeType freeBytes, FwSizeType floorBytes);

//! busyS: seconds a file buffer has sat in the LoRa file queue
Verdict downlink(bool valid, U32 busyS, U32 limitS);

Verdict watchdogPet(U32 stalledS, U32 limitS);

//! detail is millivolts
Verdict battery(bool valid, F64 volts, F64 warnVolts);

Verdict clockBoot(bool bootCountValid, FwSizeType bootCount, bool rtcFaultsRose);

enum class Quiescence { ACTIVE, OVER, UNKNOWN };

Verdict radio(Quiescence quiescence, U32 silentS, U32 limitS);

enum FlightConfigDetail : I32 {
    CFG_DISARMED = 1,
    CFG_SHORT_QUIESCENCE = 2,
    CFG_SHORT_TX_ENABLE = 4,
    CFG_NO_STARTUP_SEQ = 8,
};

//! Floors a flight unit must meet. The FPP defaults (45 min, 2800 ticks) clear them.
constexpr U32 MIN_QUIESCENCE_S = 45 * 60;
constexpr U32 MIN_TX_ENABLE_TICKS = 2700;

struct FlightConfig {
    bool armed;
    U32 quiescenceS;
    U32 txEnableTicks;
    FwSizeType startupSeqBytes;
};

Verdict flightConfig(const FlightConfig& config);

struct SensorSample {
    bool powered;
    bool ok;
    U8 group;  //!< face number, or a shared board such as the battery
};

constexpr U8 MAX_SENSOR_GROUPS = 16;

//! Bit i set when sensor i is powered and failed its read
U32 sensorFailMask(const SensorSample* samples, U32 count);

//! FAIL if any powered sensor failed. detail is sensorFailMask.
Verdict faceSensors(const SensorSample* samples, U32 count);

//! WARN when one group lost every sensor (a dead face or mux channel), FAIL when every powered group did
//! (the shared bus or mux is down). detail is the bitmask of dead groups.
Verdict i2cBus(const SensorSample* samples, U32 count);

struct Masks {
    U32 pass;
    U32 warn;
    U32 fail;
};

Masks toMasks(const CheckResult::T* results, U32 count);

}  // namespace Readiness
}  // namespace Components

#endif
