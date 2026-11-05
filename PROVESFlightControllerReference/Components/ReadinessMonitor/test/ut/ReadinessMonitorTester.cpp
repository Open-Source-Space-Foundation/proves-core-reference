// ======================================================================
// \title  ReadinessMonitorTester.cpp
// \brief  cpp file for ReadinessMonitor component test harness implementation class
// ======================================================================

#include "ReadinessMonitorTester.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <Fw/Time/TimeIntervalValueSerializableAc.hpp>
#include <Fw/Time/TimeValueSerializableAc.hpp>
#include <Fw/Types/OnEnumAc.hpp>
#include <cstdio>
#include <cstdlib>

namespace Components {

namespace {
constexpr FwChanIdType CH_FREE_SPACE = 0x10;
constexpr FwChanIdType CH_BUFF_QUEUE_DEPTH = 0x11;
constexpr U32 FILE_QUEUE_INDEX = 1;
constexpr U32 BUFF_QUEUE_COUNT = 3;
constexpr FwChanIdType CH_WDT = 0x12;
constexpr FwChanIdType CH_VOLTS = 0x13;
constexpr FwChanIdType CH_BOOT = 0x14;
constexpr FwChanIdType CH_RTC_FAULTS = 0x15;
constexpr FwChanIdType CH_BYTES_SENT = 0x16;
constexpr FwChanIdType CH_QUIESCENCE_END = 0x17;
constexpr FwChanIdType CH_LOAD_SWITCH_BASE = 0x100;
constexpr FwPrmIdType PRM_ARMED = 0x20;
constexpr FwPrmIdType PRM_QUIESCENCE = 0x21;
constexpr FwPrmIdType PRM_TX_TICKS = 0x22;

constexpr U8 BATTERY_MUX_CHANNEL = 4;
constexpr U8 FACE_OF_TEMP_SLOT[] = {0, 1, 2, 3, 5};
constexpr U8 FACE_OF_LIGHT_SLOT[] = {0, 1, 2, 3, 5};
constexpr U8 MUX_OF_LIGHT_SLOT[] = {0, 1, 2, 3, 5};

constexpr U32 BOOT_TIME_S = 1000;

U32 bit(ReadinessCheck::T check) {
    return 1U << static_cast<U32>(check);
}
}  // namespace

ReadinessMonitorTester ::ReadinessMonitorTester()
    : ReadinessMonitorGTestBase("ReadinessMonitorTester", ReadinessMonitorTester::MAX_HISTORY_SIZE),
      component("ReadinessMonitor"),
      m_sources() {
    this->initComponents();
    this->connectPorts();
    this->component.loadParameters();

    char rootTemplate[] = "/tmp/readinessXXXXXX";
    this->m_root = mkdtemp(rootTemplate);
    mkdir((this->m_root + "/seq").c_str(), 0755);
    this->writeFile("boot_count.bin", "\x01\x00\x00\x00");
    this->writeFile("seq/startup.bin", "sequence");

    this->m_sources.freeSpace = CH_FREE_SPACE;
    this->m_sources.loraBufferQueueDepth = CH_BUFF_QUEUE_DEPTH;
    this->m_sources.loraFileQueueIndex = FILE_QUEUE_INDEX;
    this->m_sources.watchdogTransitions = CH_WDT;
    this->m_sources.batteryVoltage = CH_VOLTS;
    this->m_sources.bootCount = CH_BOOT;
    this->m_sources.rtcReadFaults = CH_RTC_FAULTS;
    this->m_sources.loraBytesSent = CH_BYTES_SENT;
    this->m_sources.quiescenceEnd = CH_QUIESCENCE_END;
    this->m_sources.armed = PRM_ARMED;
    this->m_sources.quiescenceTime = PRM_QUIESCENCE;
    this->m_sources.txEnableTicks = PRM_TX_TICKS;
    for (U32 i = 0; i < FW_NUM_ARRAY_ELEMENTS(FACE_OF_TEMP_SLOT); i++) {
        this->m_sources.temp[i] = {CH_LOAD_SWITCH_BASE + FACE_OF_TEMP_SLOT[i], FACE_OF_TEMP_SLOT[i]};
    }
    for (U32 i = FW_NUM_ARRAY_ELEMENTS(FACE_OF_TEMP_SLOT); i < ReadinessMonitor::NUM_TEMPGET_OUTPUT_PORTS; i++) {
        this->m_sources.temp[i] = {0, BATTERY_MUX_CHANNEL};
    }
    for (U32 i = 0; i < ReadinessMonitor::NUM_LIGHTGET_OUTPUT_PORTS; i++) {
        this->m_sources.light[i] = {CH_LOAD_SWITCH_BASE + FACE_OF_LIGHT_SLOT[i], MUX_OF_LIGHT_SLOT[i]};
    }
    this->component.configure(this->m_root.c_str(), this->m_sources);

    // A healthy unit, all faces powered and answering, quiescence still running
    for (bool& ok : this->m_tempOk) {
        ok = true;
    }
    for (bool& ok : this->m_lightOk) {
        ok = true;
    }
    for (U8 face = 0; face < 6; face++) {
        this->powerFace(face, true);
    }
    this->setChannel<FwSizeType>(CH_FREE_SPACE, 4 * 1024 * 1024);
    this->setFileQueueDepth(0);
    this->setChannel<F64>(CH_VOLTS, 7.8);
    this->setChannel<FwSizeType>(CH_BOOT, 12);
    this->setChannel<U32>(CH_RTC_FAULTS, 0);
    this->setChannel<FwSizeType>(CH_BYTES_SENT, 0);
    this->setTestTime(Fw::Time(TimeBase::TB_PROC_TIME, 0, BOOT_TIME_S, 0));
    this->setChannel(CH_QUIESCENCE_END, Fw::TimeValue(TimeBase::TB_PROC_TIME, 0, BOOT_TIME_S + 2700, 0));
}

ReadinessMonitorTester ::~ReadinessMonitorTester() {
    this->removeFile("seq/startup.bin");
    this->removeFile("boot_count.bin");
    rmdir((this->m_root + "/seq").c_str());
    rmdir(this->m_root.c_str());
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ReadinessMonitorTester ::testHealthyUnitPassesFullRun() {
    this->runChecks();

    ASSERT_CMD_RESPONSE(0, ReadinessMonitor::OPCODE_RUN_CHECKS, 0, Fw::CmdResponse::OK);
    ASSERT_EQ(this->lastFailMask(), 0U);
    ASSERT_EQ(this->lastWarnMask(), 0U);
    const U32 expectedPass =
        bit(ReadinessCheck::FS_ROOT) | bit(ReadinessCheck::FS_SPACE) | bit(ReadinessCheck::DOWNLINK) |
        bit(ReadinessCheck::FACE_SENSORS) | bit(ReadinessCheck::I2C_BUS) | bit(ReadinessCheck::WDT_PET) |
        bit(ReadinessCheck::BATTERY) | bit(ReadinessCheck::FLIGHT_CONFIG) | bit(ReadinessCheck::CLOCK_BOOT);
    ASSERT_EQ(this->lastPassMask(), expectedPass);
    ASSERT_EVENTS_CheckDegraded_SIZE(0);
    ASSERT_EVENTS_ReadinessSummary_SIZE(1);
    ASSERT_EVENTS_ReadinessSummary(0, 9, 0, 0);
    ASSERT_TLM_SensorFailMask(0, 0U);
}

void ReadinessMonitorTester ::testFullRunLeavesUnbuiltChecksNotRun() {
    this->runChecks();

    const U32 reported = this->lastPassMask() | this->lastWarnMask() | this->lastFailMask();
    ASSERT_EQ(reported & bit(ReadinessCheck::MTQ_DRIVERS), 0U);
    ASSERT_EQ(reported & bit(ReadinessCheck::RESET_CAUSE), 0U);
    ASSERT_EQ(reported & bit(ReadinessCheck::RADIO), 0U) << "radio is not judged during quiescence";
}

void ReadinessMonitorTester ::testWatchdogStallFails() {
    this->tick(4);
    ASSERT_EQ(this->lastFailMask() & bit(ReadinessCheck::WDT_PET), 0U);

    this->m_petting = false;
    this->tick(5);

    ASSERT_NE(this->lastFailMask() & bit(ReadinessCheck::WDT_PET), 0U);
    ASSERT_EVENTS_CheckDegraded_SIZE(1);
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::WDT_PET, CheckResult::PASS, CheckResult::FAIL, 5);
}

void ReadinessMonitorTester ::testStuckFileDownlinkWarnsThenClears() {
    this->tick();
    this->setFileQueueDepth(1);
    this->tick(119);
    ASSERT_EQ(this->lastWarnMask() & bit(ReadinessCheck::DOWNLINK), 0U);

    this->tick();
    ASSERT_NE(this->lastWarnMask() & bit(ReadinessCheck::DOWNLINK), 0U);
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::DOWNLINK, CheckResult::PASS, CheckResult::WARN, 120);

    this->setFileQueueDepth(0);
    this->tick();
    ASSERT_EQ(this->lastWarnMask() & bit(ReadinessCheck::DOWNLINK), 0U);
    ASSERT_EQ(this->lastPassMask() & bit(ReadinessCheck::DOWNLINK), bit(ReadinessCheck::DOWNLINK));
}

void ReadinessMonitorTester ::testDeadFaceSensorFailsAndNamesTheSlot() {
    this->m_tempOk[2] = false;

    this->runChecks();

    ASSERT_NE(this->lastFailMask() & bit(ReadinessCheck::FACE_SENSORS), 0U);
    ASSERT_TLM_SensorFailMask(0, 1U << 2);
    ASSERT_EQ(this->lastFailMask() & bit(ReadinessCheck::I2C_BUS), 0U) << "face 2's light sensor still answers";
    ASSERT_EQ(this->lastWarnMask() & bit(ReadinessCheck::I2C_BUS), 0U);
}

void ReadinessMonitorTester ::testUnpoweredFaceIsNotRead() {
    this->powerFace(2, false);
    this->m_tempOk[2] = false;
    this->m_lightOk[2] = false;

    this->runChecks();

    ASSERT_EQ(this->lastFailMask(), 0U);
    ASSERT_from_tempGet_SIZE(ReadinessMonitor::NUM_TEMPGET_OUTPUT_PORTS - 1);
    ASSERT_from_lightGet_SIZE(ReadinessMonitor::NUM_LIGHTGET_OUTPUT_PORTS - 1);
}

void ReadinessMonitorTester ::testAllSensorsDeadIsABusFault() {
    this->m_tempOk[1] = false;
    this->m_lightOk[1] = false;
    this->runChecks();
    ASSERT_NE(this->lastWarnMask() & bit(ReadinessCheck::I2C_BUS), 0U) << "one mux channel down is a face fault";

    for (bool& ok : this->m_tempOk) {
        ok = false;
    }
    for (bool& ok : this->m_lightOk) {
        ok = false;
    }
    this->runChecks();
    ASSERT_NE(this->lastFailMask() & bit(ReadinessCheck::I2C_BUS), 0U) << "every channel down is a bus fault";
}

void ReadinessMonitorTester ::testDisarmedUnitFailsFlightConfig() {
    this->setParam<bool>(PRM_ARMED, false);
    this->setParam(PRM_QUIESCENCE, Fw::TimeIntervalValue(60, 0));

    this->runChecks();

    ASSERT_NE(this->lastFailMask() & bit(ReadinessCheck::FLIGHT_CONFIG), 0U);
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::FLIGHT_CONFIG, CheckResult::NOT_RUN, CheckResult::FAIL,
                                Readiness::CFG_DISARMED | Readiness::CFG_SHORT_QUIESCENCE);
}

void ReadinessMonitorTester ::testMissingStartupSequenceFailsFlightConfig() {
    this->removeFile("seq/startup.bin");

    this->runChecks();

    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::FLIGHT_CONFIG, CheckResult::NOT_RUN, CheckResult::FAIL,
                                Readiness::CFG_NO_STARTUP_SEQ);
}

void ReadinessMonitorTester ::testGarbageRootEntryFailsFsRoot() {
    this->writeFile("\x01\x7f", "");

    this->runChecks();

    this->removeFile("\x01\x7f");
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::FS_ROOT, CheckResult::NOT_RUN, CheckResult::FAIL,
                                Readiness::ROOT_GARBAGE_NAME);
}

void ReadinessMonitorTester ::testMissingBootCountFailsFsRoot() {
    this->removeFile("boot_count.bin");

    this->runChecks();

    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::FS_ROOT, CheckResult::NOT_RUN, CheckResult::FAIL,
                                Readiness::ROOT_NO_BOOT_COUNT);
}

void ReadinessMonitorTester ::testRadioSilenceWarnsOnlyAfterQuiescence() {
    this->tick(1000);
    ASSERT_EQ((this->lastPassMask() | this->lastWarnMask()) & bit(ReadinessCheck::RADIO), 0U);

    this->setTestTime(Fw::Time(TimeBase::TB_PROC_TIME, 0, BOOT_TIME_S + 2700, 0));
    this->tick(600);
    ASSERT_EQ(this->lastPassMask() & bit(ReadinessCheck::RADIO), bit(ReadinessCheck::RADIO));

    this->tick();
    ASSERT_NE(this->lastWarnMask() & bit(ReadinessCheck::RADIO), 0U);

    this->setChannel<FwSizeType>(CH_BYTES_SENT, 300);
    this->tick();
    ASSERT_EQ(this->lastPassMask() & bit(ReadinessCheck::RADIO), bit(ReadinessCheck::RADIO));
}

void ReadinessMonitorTester ::testBatteryMissingFailsAndLowWarns() {
    this->clearChannel(CH_VOLTS);
    this->tick();
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::BATTERY, CheckResult::NOT_RUN, CheckResult::FAIL,
                                Readiness::DETAIL_NO_DATA);

    this->setChannel<F64>(CH_VOLTS, 6.6);
    this->tick();
    ASSERT_EVENTS_CheckDegraded(1, ReadinessCheck::BATTERY, CheckResult::FAIL, CheckResult::WARN, 6600);
}

void ReadinessMonitorTester ::testLowFreeSpaceWarns() {
    this->setChannel<FwSizeType>(CH_FREE_SPACE, 100 * 1024);
    this->tick();
    ASSERT_EVENTS_CheckDegraded(0, ReadinessCheck::FS_SPACE, CheckResult::NOT_RUN, CheckResult::WARN, 100);
}

void ReadinessMonitorTester ::testBootTriggersOneFullRun() {
    this->tick(119);
    ASSERT_EVENTS_ReadinessSummary_SIZE(0);
    ASSERT_from_tempGet_SIZE(0);

    this->tick();
    ASSERT_EVENTS_ReadinessSummary_SIZE(1);

    this->tick(200);
    ASSERT_EVENTS_ReadinessSummary_SIZE(1);
}

void ReadinessMonitorTester ::testPingIsAnswered() {
    this->invoke_to_pingIn(0, 0xCAFE);
    ASSERT_EQ(this->dispatchOne(this->component), ReadinessMonitorComponentBase::MSG_DISPATCH_OK);
    ASSERT_from_pingOut_SIZE(1);
    ASSERT_from_pingOut(0, 0xCAFE);
}

// ----------------------------------------------------------------------
// Port handlers
// ----------------------------------------------------------------------

F64 ReadinessMonitorTester ::from_tempGet_handler(FwIndexType portNum, Fw::Success& condition) {
    this->pushFromPortEntry_tempGet(condition);
    condition = this->m_tempOk[portNum] ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
    return 20.0;
}

F32 ReadinessMonitorTester ::from_lightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    this->pushFromPortEntry_lightGet(condition);
    condition = this->m_lightOk[portNum] ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
    return 100.0f;
}

Fw::TlmValid ReadinessMonitorTester ::from_tlmGet_handler(FwIndexType portNum,
                                                          FwChanIdType id,
                                                          Fw::Time& timeTag,
                                                          Fw::TlmBuffer& val) {
    const auto entry = this->m_channels.find(id);
    if (entry == this->m_channels.end()) {
        val.resetSer();
        return Fw::TlmValid::INVALID;
    }
    val = entry->second;
    return Fw::TlmValid::VALID;
}

Fw::ParamValid ReadinessMonitorTester ::from_prmDbGet_handler(FwIndexType portNum,
                                                              FwPrmIdType id,
                                                              Fw::ParamBuffer& val) {
    const auto entry = this->m_params.find(id);
    if (entry == this->m_params.end()) {
        return Fw::ParamValid::INVALID;
    }
    val = entry->second;
    return Fw::ParamValid::VALID;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

template <typename T>
void ReadinessMonitorTester ::setChannel(FwChanIdType id, const T& value) {
    Fw::TlmBuffer buffer;
    ASSERT_EQ(buffer.serializeFrom(value), Fw::FW_SERIALIZE_OK);
    this->m_channels[id] = buffer;
}

void ReadinessMonitorTester ::clearChannel(FwChanIdType id) {
    this->m_channels.erase(id);
}

template <typename T>
void ReadinessMonitorTester ::setParam(FwPrmIdType id, const T& value) {
    Fw::ParamBuffer buffer;
    ASSERT_EQ(buffer.serializeFrom(value), Fw::FW_SERIALIZE_OK);
    this->m_params[id] = buffer;
}

void ReadinessMonitorTester ::setFileQueueDepth(U32 depth) {
    Fw::TlmBuffer buffer;
    for (U32 i = 0; i < BUFF_QUEUE_COUNT; i++) {
        ASSERT_EQ(buffer.serializeFrom(static_cast<U32>(i == FILE_QUEUE_INDEX ? depth : 0)), Fw::FW_SERIALIZE_OK);
    }
    this->m_channels[CH_BUFF_QUEUE_DEPTH] = buffer;
}

void ReadinessMonitorTester ::powerFace(U8 face, bool on) {
    this->setChannel(CH_LOAD_SWITCH_BASE + face, on ? Fw::On(Fw::On::ON) : Fw::On(Fw::On::OFF));
}

void ReadinessMonitorTester ::runChecks() {
    this->sendCmd_RUN_CHECKS(0, 0);
    ASSERT_EQ(this->dispatchOne(this->component), ReadinessMonitorComponentBase::MSG_DISPATCH_OK);
}

void ReadinessMonitorTester ::tick(U32 count) {
    for (U32 i = 0; i < count; i++) {
        if (this->m_petting) {
            this->m_transitions++;
            this->setChannel<U32>(CH_WDT, this->m_transitions);
        }
        this->invoke_to_run(0, 0);
        ASSERT_EQ(this->dispatchOne(this->component), ReadinessMonitorComponentBase::MSG_DISPATCH_OK);
    }
}

void ReadinessMonitorTester ::writeFile(const char* leaf, const char* contents) {
    FILE* file = fopen((this->m_root + "/" + leaf).c_str(), "w");
    ASSERT_NE(file, nullptr);
    fputs(contents, file);
    fclose(file);
}

void ReadinessMonitorTester ::removeFile(const char* leaf) {
    (void)remove((this->m_root + "/" + leaf).c_str());
}

U32 ReadinessMonitorTester ::lastPassMask() {
    return this->tlmHistory_PassMask->size() == 0
               ? 0U
               : this->tlmHistory_PassMask->at(this->tlmHistory_PassMask->size() - 1).arg;
}

U32 ReadinessMonitorTester ::lastWarnMask() {
    return this->tlmHistory_WarnMask->size() == 0
               ? 0U
               : this->tlmHistory_WarnMask->at(this->tlmHistory_WarnMask->size() - 1).arg;
}

U32 ReadinessMonitorTester ::lastFailMask() {
    return this->tlmHistory_FailMask->size() == 0
               ? 0U
               : this->tlmHistory_FailMask->at(this->tlmHistory_FailMask->size() - 1).arg;
}

}  // namespace Components
