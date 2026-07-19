// ======================================================================
// \title  MosaicManager.hpp
// \brief  hpp file for MosaicManager component implementation class
//         Receives MOSAIC gamma ray detector data over UART and stores
//         it on disk as F Prime data products
// ======================================================================

#ifndef Components_MosaicManager_HPP
#define Components_MosaicManager_HPP

#include "PROVESFlightControllerReference/Components/MosaicManager/MosaicManagerComponentAc.hpp"

namespace Components {

class MosaicManager final : public MosaicManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct MosaicManager object
    MosaicManager(const char* const compName  //!< The component name
    );

    //! Destroy MosaicManager object
    ~MosaicManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //! Receives raw byte stream from the MOSAIC UART driver
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& buffer,
                        const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for run
    //! Periodic telemetry and data product flush
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command START_RECORDING
    void START_RECORDING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeq            //!< The command sequence number
                                    ) override;

    //! Handler implementation for command STOP_RECORDING
    void STOP_RECORDING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq            //!< The command sequence number
                                   ) override;

    //! Handler implementation for command FLUSH
    void FLUSH_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Process one completed line from the payload
    void processLine();

    //! Record a parsed sample into the current data product container
    void recordSample(const MosaicManager_MosaicSample& sample);

    //! Get a fresh data product container if one is not already open
    //! Returns true if a container is available
    bool ensureContainer();

    //! Send the current container to be written to disk and reset state
    void sendContainer();

    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Maximum length of one "ADC=...,MV=..." line, including newline
    static constexpr U32 MAX_LINE_LENGTH = 64;

    //! Samples collected per data product container
    static constexpr U32 SAMPLES_PER_CONTAINER = 100;

    //! Flush a partially filled container after this many seconds without filling
    static constexpr U32 FLUSH_TIMEOUT_SECONDS = 60;

    //! Serialized size of one sample record (record id + struct data)
    static constexpr FwSizeType RECORD_SIZE = sizeof(FwDpIdType) + MosaicManager_MosaicSample::SERIALIZED_SIZE;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Line accumulation buffer for the ASCII CSV protocol
    U8 m_lineBuffer[MAX_LINE_LENGTH];
    U32 m_lineLength = 0;

    //! Current data product container
    DpContainer m_container;
    bool m_containerOpen = false;
    U32 m_samplesInContainer = 0;

    //! Time (seconds) when the current container received its first sample
    U32 m_containerStartSeconds = 0;

    //! Whether samples are recorded into data products
    bool m_recording = true;

    //! Counters for telemetry
    U32 m_samplesRecorded = 0;
    U32 m_productsSent = 0;
    U32 m_parseErrors = 0;
};

}  // namespace Components

#endif
