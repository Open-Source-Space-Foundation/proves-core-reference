// ======================================================================
// \title  ReadinessMonitor.hpp
// \brief  hpp file for ReadinessMonitor component implementation class
// ======================================================================

#ifndef Components_ReadinessMonitor_HPP
#define Components_ReadinessMonitor_HPP

#include <Fw/Types/String.hpp>

#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessChecks.hpp"
#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessMonitorComponentAc.hpp"

namespace Components {

class ReadinessMonitor final : public ReadinessMonitorComponentBase {
  public:
    //! A sensor behind a load switch. loadSwitchIsOn is the LoadSwitch IsOn channel, or 0 when always powered.
    struct SensorSlot {
        FwChanIdType loadSwitchIsOn;
        U8 group;  //!< I2C mux channel, so sensors that share a channel share a group
    };

    //! Absolute IDs of the channels and parameters the checks read. An ID of 0 reads as "no data".
    struct Sources {
        FwChanIdType freeSpace;                       //!< FsSpace.FreeSpace
        FwChanIdType loraBufferQueueDepth;            //!< ComQueue.buffQueueDepth of the LoRa queue
        U32 loraFileQueueIndex;                       //!< index of the FILE queue inside buffQueueDepth
        FwChanIdType watchdogTransitions;             //!< Watchdog.WatchdogTransitions
        FwChanIdType batteryVoltage;                  //!< Ina219Manager.Voltage on the system rail
        FwChanIdType bootCount;                       //!< StartupManager.BootCount
        FwChanIdType rtcReadFaults;                   //!< RtcManager.DisciplineReadFaults
        FwChanIdType loraBytesSent;                   //!< LoRa.BytesSent
        FwChanIdType quiescenceEnd;                   //!< StartupManager.QuiescenceEndTime
        FwPrmIdType armed;                            //!< StartupManager.ARMED
        FwPrmIdType quiescenceTime;                   //!< StartupManager.QUIESCENCE_TIME
        FwPrmIdType txEnableTicks;                    //!< StartupManager.TRANSMIT_ENABLE_TICKS
        SensorSlot temp[NUM_TEMPGET_OUTPUT_PORTS];    //!< matches tempGet port order
        SensorSlot light[NUM_LIGHTGET_OUTPUT_PORTS];  //!< matches lightGet port order
    };

    explicit ReadinessMonitor(const char* const compName);

    ~ReadinessMonitor();

    //! \param fsRoot directory to list for FS_ROOT, and the base of boot_count.bin and seq/startup.bin
    void configure(const char* fsRoot, const Sources& sources);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;

    void pingIn_handler(FwIndexType portNum, U32 key) override;

    void RUN_CHECKS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Checks that only read telemetry. elapsedS feeds the stall timers.
    void runCheapChecks(U32 elapsedS);

    //! Root listing, flight config, and sensor reads, then masks and the summary
    void runFullChecks();

    void record(ReadinessCheck::T check, const Readiness::Verdict& verdict);

    void publishMasks();

    Readiness::RootListing listRoot();

    Readiness::FlightConfig readFlightConfig();

    void pathUnderRoot(Fw::String& path, const char* leaf) const;

    Readiness::Quiescence quiescence();

    void sampleSensors(Readiness::SensorSample* samples);

    bool isPowered(FwChanIdType loadSwitchIsOn);

    //! Latest serialized value of a channel. False when the channel has no value yet.
    bool readChannel(FwChanIdType id, Fw::TlmBuffer& value);

    template <typename T>
    bool readChannelAs(FwChanIdType id, T& out);

    template <typename T>
    bool readParamAs(FwPrmIdType id, T& out);

    Fw::String m_fsRoot;
    Sources m_sources;
    CheckResult::T m_results[ReadinessCheck::NUM_CONSTANTS];
    U32 m_uptimeS;
    U32 m_downlinkBusyS;
    Readiness::StallTimer m_watchdogStall;
    Readiness::StallTimer m_radioStall;
    bool m_haveRtcFaults;
    U32 m_rtcFaults;
};

}  // namespace Components

#endif
