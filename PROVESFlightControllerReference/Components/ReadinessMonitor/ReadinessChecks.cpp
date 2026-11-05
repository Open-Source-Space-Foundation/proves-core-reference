// ======================================================================
// \title  ReadinessChecks.cpp
// \brief  Pure verdict functions for ReadinessMonitor
// ======================================================================

#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessChecks.hpp"

namespace Components {
namespace Readiness {

namespace {
Verdict pass(I32 detail = 0) {
    return {CheckResult::PASS, detail};
}
Verdict warn(I32 detail) {
    return {CheckResult::WARN, detail};
}
Verdict fail(I32 detail) {
    return {CheckResult::FAIL, detail};
}
I32 clampToI32(U64 value) {
    return (value > 0x7FFFFFFFULL) ? 0x7FFFFFFF : static_cast<I32>(value);
}
}  // namespace

U32 StallTimer::update(bool valid, U64 value, U32 elapsedS) {
    if (valid && (!this->m_seen || value != this->m_last)) {
        this->m_seen = true;
        this->m_last = value;
        this->m_stalledS = 0;
    } else {
        this->m_stalledS += elapsedS;
    }
    return this->m_stalledS;
}

U32 accumulate(bool condition, U32 priorS, U32 elapsedS) {
    return condition ? priorS + elapsedS : 0;
}

bool isGarbageName(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return true;
    }
    for (const char* c = name; *c != '\0'; c++) {
        const U8 byte = static_cast<U8>(*c);
        if (byte < 0x20 || byte > 0x7E) {
            return true;
        }
    }
    return false;
}

Verdict fsRoot(const RootListing& listing) {
    if (!listing.readable) {
        return fail(ROOT_UNREADABLE);
    }
    I32 detail = 0;
    if (listing.garbageNames > 0) {
        detail |= ROOT_GARBAGE_NAME;
    }
    if (!listing.bootCountPresent) {
        detail |= ROOT_NO_BOOT_COUNT;
    }
    return (detail == 0) ? pass() : fail(detail);
}

Verdict fsSpace(bool valid, FwSizeType freeBytes, FwSizeType floorBytes) {
    if (!valid) {
        return fail(DETAIL_NO_DATA);
    }
    const I32 freeKiB = clampToI32(static_cast<U64>(freeBytes) / 1024);
    return (freeBytes < floorBytes) ? warn(freeKiB) : pass(freeKiB);
}

Verdict downlink(bool valid, U32 busyS, U32 limitS) {
    if (!valid) {
        return warn(DETAIL_NO_DATA);
    }
    return (busyS >= limitS) ? warn(clampToI32(busyS)) : pass(clampToI32(busyS));
}

Verdict watchdogPet(U32 stalledS, U32 limitS) {
    return (stalledS >= limitS) ? fail(clampToI32(stalledS)) : pass();
}

Verdict battery(bool valid, F64 volts, F64 warnVolts) {
    if (!valid) {
        return fail(DETAIL_NO_DATA);
    }
    const I32 millivolts = (volts <= 0.0) ? 0 : clampToI32(static_cast<U64>(volts * 1000.0 + 0.5));
    return (volts < warnVolts) ? warn(millivolts) : pass(millivolts);
}

Verdict clockBoot(bool bootCountValid, FwSizeType bootCount, bool rtcFaultsRose) {
    if (!bootCountValid || bootCount == 0) {
        return fail(DETAIL_NO_DATA);
    }
    return rtcFaultsRose ? warn(1) : pass();
}

Verdict radio(Quiescence quiescence, U32 silentS, U32 limitS) {
    if (quiescence != Quiescence::OVER) {
        return {CheckResult::NOT_RUN, 0};
    }
    return (silentS >= limitS) ? warn(clampToI32(silentS)) : pass();
}

Verdict flightConfig(const FlightConfig& config) {
    I32 detail = 0;
    if (!config.armed) {
        detail |= CFG_DISARMED;
    }
    if (config.quiescenceS < MIN_QUIESCENCE_S) {
        detail |= CFG_SHORT_QUIESCENCE;
    }
    if (config.txEnableTicks < MIN_TX_ENABLE_TICKS) {
        detail |= CFG_SHORT_TX_ENABLE;
    }
    if (config.startupSeqBytes == 0) {
        detail |= CFG_NO_STARTUP_SEQ;
    }
    return (detail == 0) ? pass() : fail(detail);
}

U32 sensorFailMask(const SensorSample* samples, U32 count) {
    U32 mask = 0;
    for (U32 i = 0; i < count && i < 32; i++) {
        if (samples[i].powered && !samples[i].ok) {
            mask |= (1U << i);
        }
    }
    return mask;
}

Verdict faceSensors(const SensorSample* samples, U32 count) {
    bool anyPowered = false;
    for (U32 i = 0; i < count; i++) {
        anyPowered = anyPowered || samples[i].powered;
    }
    if (!anyPowered) {
        return {CheckResult::NOT_RUN, 0};
    }
    const U32 mask = sensorFailMask(samples, count);
    return (mask == 0) ? pass() : fail(static_cast<I32>(mask));
}

Verdict i2cBus(const SensorSample* samples, U32 count) {
    U32 poweredGroups = 0;
    U32 answeringGroups = 0;
    for (U32 i = 0; i < count; i++) {
        if (samples[i].powered && samples[i].group < MAX_SENSOR_GROUPS) {
            poweredGroups |= (1U << samples[i].group);
            if (samples[i].ok) {
                answeringGroups |= (1U << samples[i].group);
            }
        }
    }
    if (poweredGroups == 0) {
        return {CheckResult::NOT_RUN, 0};
    }
    const U32 deadGroups = poweredGroups & ~answeringGroups;
    if (deadGroups == 0) {
        return pass();
    }
    return (answeringGroups == 0) ? fail(static_cast<I32>(deadGroups)) : warn(static_cast<I32>(deadGroups));
}

Masks toMasks(const CheckResult::T* results, U32 count) {
    Masks masks = {0, 0, 0};
    for (U32 i = 0; i < count && i < 32; i++) {
        const U32 bit = 1U << i;
        switch (results[i]) {
            case CheckResult::PASS:
                masks.pass |= bit;
                break;
            case CheckResult::WARN:
                masks.warn |= bit;
                break;
            case CheckResult::FAIL:
                masks.fail |= bit;
                break;
            default:
                break;
        }
    }
    return masks;
}

}  // namespace Readiness
}  // namespace Components
