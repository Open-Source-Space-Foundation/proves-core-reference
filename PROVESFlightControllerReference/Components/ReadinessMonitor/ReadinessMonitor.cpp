// ======================================================================
// \title  ReadinessMonitor.cpp
// \brief  cpp file for ReadinessMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessMonitor.hpp"

#include <Fw/Time/TimeIntervalValueSerializableAc.hpp>
#include <Fw/Time/TimeValueSerializableAc.hpp>
#include <Fw/Types/OnEnumAc.hpp>
#include <Os/Directory.hpp>
#include <Os/FileSystem.hpp>

namespace Components {

using Readiness::Verdict;

ReadinessMonitor ::ReadinessMonitor(const char* const compName)
    : ReadinessMonitorComponentBase(compName),
      m_sources(),
      m_uptimeS(0),
      m_downlinkBusyS(0),
      m_haveRtcFaults(false),
      m_rtcFaults(0) {
    for (FwIndexType i = 0; i < ReadinessCheck::NUM_CONSTANTS; i++) {
        this->m_results[i] = CheckResult::NOT_RUN;
    }
}

ReadinessMonitor ::~ReadinessMonitor() {}

void ReadinessMonitor ::configure(const char* fsRoot, const Sources& sources) {
    this->m_fsRoot = fsRoot;
    this->m_sources = sources;
}

// ----------------------------------------------------------------------
// Handlers
// ----------------------------------------------------------------------

void ReadinessMonitor ::run_handler(FwIndexType portNum, U32 context) {
    this->m_uptimeS++;
    this->runCheapChecks(1);
    Fw::ParamValid valid;
    const U32 bootRunDelay = this->paramGet_BOOT_FULL_RUN_DELAY_S(valid);
    if (bootRunDelay != 0 && this->m_uptimeS == bootRunDelay) {
        this->runFullChecks();
    } else {
        this->publishMasks();
    }
}

void ReadinessMonitor ::pingIn_handler(FwIndexType portNum, U32 key) {
    this->pingOut_out(0, key);
}

void ReadinessMonitor ::RUN_CHECKS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // A command is not a tick, so the stall timers do not advance
    this->runCheapChecks(0);
    this->runFullChecks();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Check passes
// ----------------------------------------------------------------------

void ReadinessMonitor ::runCheapChecks(U32 elapsedS) {
    Fw::ParamValid valid;

    FwSizeType freeBytes = 0;
    const bool haveFree = this->readChannelAs(this->m_sources.freeSpace, freeBytes);
    this->record(ReadinessCheck::FS_SPACE,
                 Readiness::fsSpace(haveFree, freeBytes, this->paramGet_FS_FREE_FLOOR_BYTES(valid)));

    Fw::TlmBuffer depths;
    U32 fileDepth = 0;
    bool haveDepth = this->readChannel(this->m_sources.loraBufferQueueDepth, depths);
    for (U32 i = 0; haveDepth && i <= this->m_sources.loraFileQueueIndex; i++) {
        haveDepth = (depths.deserializeTo(fileDepth) == Fw::FW_SERIALIZE_OK);
    }
    this->m_downlinkBusyS = Readiness::accumulate(haveDepth && fileDepth > 0, this->m_downlinkBusyS, elapsedS);
    this->record(ReadinessCheck::DOWNLINK,
                 Readiness::downlink(haveDepth, this->m_downlinkBusyS, this->paramGet_DOWNLINK_STALL_S(valid)));

    U32 transitions = 0;
    const bool haveTransitions = this->readChannelAs(this->m_sources.watchdogTransitions, transitions);
    const U32 petStalledS = this->m_watchdogStall.update(haveTransitions, transitions, elapsedS);
    this->record(ReadinessCheck::WDT_PET, Readiness::watchdogPet(petStalledS, this->paramGet_WDT_STALL_S(valid)));

    F64 volts = 0.0;
    const bool haveVolts = this->readChannelAs(this->m_sources.batteryVoltage, volts);
    this->record(ReadinessCheck::BATTERY, Readiness::battery(haveVolts, volts, this->paramGet_VBATT_WARN_V(valid)));

    FwSizeType bootCount = 0;
    const bool haveBootCount = this->readChannelAs(this->m_sources.bootCount, bootCount);
    U32 rtcFaults = 0;
    const bool haveRtcFaults = this->readChannelAs(this->m_sources.rtcReadFaults, rtcFaults);
    const bool rtcFaultsRose = haveRtcFaults && this->m_haveRtcFaults && rtcFaults > this->m_rtcFaults;
    if (haveRtcFaults) {
        this->m_haveRtcFaults = true;
        this->m_rtcFaults = rtcFaults;
    }
    this->record(ReadinessCheck::CLOCK_BOOT, Readiness::clockBoot(haveBootCount, bootCount, rtcFaultsRose));

    const Readiness::Quiescence quiet = this->quiescence();
    U32 silentS = 0;
    if (quiet == Readiness::Quiescence::OVER) {
        FwSizeType bytesSent = 0;
        const bool haveBytes = this->readChannelAs(this->m_sources.loraBytesSent, bytesSent);
        silentS = this->m_radioStall.update(haveBytes, bytesSent, elapsedS);
    } else {
        // The silence window opens when quiescence ends, not at boot
        this->m_radioStall = Readiness::StallTimer();
    }
    this->record(ReadinessCheck::RADIO, Readiness::radio(quiet, silentS, this->paramGet_RADIO_SILENT_S(valid)));
}

void ReadinessMonitor ::runFullChecks() {
    this->record(ReadinessCheck::FS_ROOT, Readiness::fsRoot(this->listRoot()));
    this->record(ReadinessCheck::FLIGHT_CONFIG, Readiness::flightConfig(this->readFlightConfig()));

    Readiness::SensorSample samples[NUM_TEMPGET_OUTPUT_PORTS + NUM_LIGHTGET_OUTPUT_PORTS];
    this->sampleSensors(samples);
    const U32 count = static_cast<U32>(FW_NUM_ARRAY_ELEMENTS(samples));
    this->record(ReadinessCheck::FACE_SENSORS, Readiness::faceSensors(samples, count));
    this->record(ReadinessCheck::I2C_BUS, Readiness::i2cBus(samples, count));
    this->tlmWrite_SensorFailMask(Readiness::sensorFailMask(samples, count));

    this->publishMasks();

    const Readiness::Masks masks = Readiness::toMasks(this->m_results, static_cast<U32>(ReadinessCheck::NUM_CONSTANTS));
    this->log_ACTIVITY_HI_ReadinessSummary(static_cast<U8>(__builtin_popcount(masks.pass)),
                                           static_cast<U8>(__builtin_popcount(masks.warn)),
                                           static_cast<U8>(__builtin_popcount(masks.fail)));
}

void ReadinessMonitor ::record(ReadinessCheck::T check, const Verdict& verdict) {
    const CheckResult::T was = this->m_results[check];
    if (was == verdict.result) {
        return;
    }
    this->m_results[check] = verdict.result;
    if (verdict.result == CheckResult::WARN || verdict.result == CheckResult::FAIL) {
        this->log_WARNING_HI_CheckDegraded(check, was, verdict.result, verdict.detail);
    } else {
        this->log_ACTIVITY_HI_CheckCleared(check, was, verdict.result, verdict.detail);
    }
}

void ReadinessMonitor ::publishMasks() {
    const Readiness::Masks masks = Readiness::toMasks(this->m_results, static_cast<U32>(ReadinessCheck::NUM_CONSTANTS));
    this->tlmWrite_PassMask(masks.pass);
    this->tlmWrite_WarnMask(masks.warn);
    this->tlmWrite_FailMask(masks.fail);
}

// ----------------------------------------------------------------------
// Inputs
// ----------------------------------------------------------------------

Readiness::RootListing ReadinessMonitor ::listRoot() {
    Readiness::RootListing listing = {false, 0, false};
    Os::Directory root;
    if (root.open(this->m_fsRoot.toChar(), Os::Directory::READ) != Os::Directory::OP_OK) {
        return listing;
    }
    listing.readable = true;
    char name[256];
    while (root.read(name, sizeof(name)) == Os::Directory::OP_OK) {
        if (Readiness::isGarbageName(name)) {
            listing.garbageNames++;
        }
    }
    root.close();

    Fw::String bootCountPath;
    this->pathUnderRoot(bootCountPath, "boot_count.bin");
    listing.bootCountPresent = (Os::FileSystem::getPathType(bootCountPath.toChar()) == Os::FileSystem::FILE);
    return listing;
}

void ReadinessMonitor ::pathUnderRoot(Fw::String& path, const char* leaf) const {
    const char* root = this->m_fsRoot.toChar();
    const FwSizeType length = this->m_fsRoot.length();
    const char* separator = (length > 0 && root[length - 1] == '/') ? "" : "/";
    path.format("%s%s%s", root, separator, leaf);
}

Readiness::FlightConfig ReadinessMonitor ::readFlightConfig() {
    // A parameter missing from PrmDb runs at its FPP default, which is the flight value
    Readiness::FlightConfig config = {true, Readiness::MIN_QUIESCENCE_S, Readiness::MIN_TX_ENABLE_TICKS, 0};
    (void)this->readParamAs(this->m_sources.armed, config.armed);
    Fw::TimeIntervalValue quiescence;
    if (this->readParamAs(this->m_sources.quiescenceTime, quiescence)) {
        config.quiescenceS = quiescence.get_seconds();
    }
    (void)this->readParamAs(this->m_sources.txEnableTicks, config.txEnableTicks);

    Fw::String seqPath;
    this->pathUnderRoot(seqPath, "seq/startup.bin");
    FwSizeType size = 0;
    if (Os::FileSystem::getFileSize(seqPath.toChar(), size) == Os::FileSystem::OP_OK) {
        config.startupSeqBytes = size;
    }
    return config;
}

Readiness::Quiescence ReadinessMonitor ::quiescence() {
    Fw::TimeValue end;
    if (!this->readChannelAs(this->m_sources.quiescenceEnd, end)) {
        return Readiness::Quiescence::UNKNOWN;
    }
    const Fw::Time endTime(end.get_timeBase(), end.get_timeContext(), end.get_seconds(), end.get_useconds());
    switch (Fw::Time::compare(this->getTime(), endTime)) {
        case Fw::TimeComparison::GT:
        case Fw::TimeComparison::EQ:
            return Readiness::Quiescence::OVER;
        case Fw::TimeComparison::LT:
            return Readiness::Quiescence::ACTIVE;
        default:
            return Readiness::Quiescence::UNKNOWN;
    }
}

void ReadinessMonitor ::sampleSensors(Readiness::SensorSample* samples) {
    for (FwIndexType i = 0; i < NUM_TEMPGET_OUTPUT_PORTS; i++) {
        const SensorSlot& slot = this->m_sources.temp[i];
        Readiness::SensorSample& sample = samples[i];
        sample = {false, false, slot.group};
        if (this->isConnected_tempGet_OutputPort(i) && this->isPowered(slot.loadSwitchIsOn)) {
            Fw::Success condition = Fw::Success::FAILURE;
            (void)this->tempGet_out(i, condition);
            sample.powered = true;
            sample.ok = (condition == Fw::Success::SUCCESS);
        }
    }
    for (FwIndexType i = 0; i < NUM_LIGHTGET_OUTPUT_PORTS; i++) {
        const SensorSlot& slot = this->m_sources.light[i];
        Readiness::SensorSample& sample = samples[NUM_TEMPGET_OUTPUT_PORTS + i];
        sample = {false, false, slot.group};
        if (this->isConnected_lightGet_OutputPort(i) && this->isPowered(slot.loadSwitchIsOn)) {
            Fw::Success condition = Fw::Success::FAILURE;
            (void)this->lightGet_out(i, condition);
            sample.powered = true;
            sample.ok = (condition == Fw::Success::SUCCESS);
        }
    }
}

bool ReadinessMonitor ::isPowered(FwChanIdType loadSwitchIsOn) {
    if (loadSwitchIsOn == 0) {
        return true;
    }
    Fw::On state = Fw::On::OFF;
    return this->readChannelAs(loadSwitchIsOn, state) && state == Fw::On::ON;
}

bool ReadinessMonitor ::readChannel(FwChanIdType id, Fw::TlmBuffer& value) {
    if (id == 0 || !this->isConnected_tlmGet_OutputPort(0)) {
        return false;
    }
    Fw::Time timeTag;
    return this->tlmGet_out(0, id, timeTag, value) == Fw::TlmValid::VALID;
}

template <typename T>
bool ReadinessMonitor ::readChannelAs(FwChanIdType id, T& out) {
    Fw::TlmBuffer value;
    return this->readChannel(id, value) && value.deserializeTo(out) == Fw::FW_SERIALIZE_OK;
}

template <typename T>
bool ReadinessMonitor ::readParamAs(FwPrmIdType id, T& out) {
    if (id == 0 || !this->isConnected_prmDbGet_OutputPort(0)) {
        return false;
    }
    Fw::ParamBuffer value;
    if (this->prmDbGet_out(0, id, value) != Fw::ParamValid::VALID) {
        return false;
    }
    T parsed;
    if (value.deserializeTo(parsed) != Fw::FW_SERIALIZE_OK) {
        return false;
    }
    out = parsed;
    return true;
}

}  // namespace Components
