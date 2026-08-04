// ======================================================================
// \title  MosaicManager.hpp
// \brief  hpp file for MosaicManager component implementation class
//         Receives MOSAIC gamma ray detector data over UART and stores
//         it on disk under /mosaic for later downlink
// ======================================================================

#ifndef Components_MosaicManager_HPP
#define Components_MosaicManager_HPP

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"
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

    //! Initialize the component
    void init(FwEnumStoreType instance = 0  //!< Instance ID
    );

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

    //! Record a parsed sample into the current sample file
    void recordSample(U16 adc, U16 millivolts);

    //! Write all buffered samples to the current file in one filesystem operation
    //! Returns true when the full batch was written
    bool writeBufferedSamples();

    //! Count a filesystem error and stop recording when the configured limit is reached
    void recordFilesystemError();

    //! Open a fresh sample file if one is not already open
    //! Returns true if a file is available for writing
    bool ensureFileOpen();

    //! Flush and close the current sample file and reset state
    //! \param complete true when the file holds only whole records and is ready
    //!        for downlink, so SampleFileClosed is reported; false when it is
    //!        being abandoned after a write failure
    void closeFile(bool complete = true);

    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Maximum length of one "ADC=...,MV=..." line, including newline
    static constexpr U32 MAX_LINE_LENGTH = 64;

    //! Flush a partially filled file after this many seconds without filling
    static constexpr U32 FLUSH_TIMEOUT_SECONDS = 60;

    //! Directory where sample files are stored for later downlink
    static constexpr const char* SAMPLE_DIR = "/mosaic";

    //! Serialized size of one sample record: U32 time seconds, then raw ADC code and millivolts, both U16
    static constexpr FwSizeType RECORD_SIZE = sizeof(U32) + 2 * sizeof(U16);

    //! Maximum batch size representable by the U8 SAMPLES_PER_WRITE parameter
    static constexpr FwSizeType WRITE_BUFFER_SAMPLE_CAPACITY = 0xFF;

    //! Size of the batched filesystem write buffer
    static constexpr FwSizeType WRITE_BUFFER_SIZE = WRITE_BUFFER_SAMPLE_CAPACITY * RECORD_SIZE;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Line accumulation buffer for the ASCII CSV protocol
    U8 m_lineBuffer[MAX_LINE_LENGTH];
    U32 m_lineLength = 0;

    //! Current sample file
    Os::File m_file;
    bool m_fileOpen = false;
    U32 m_samplesInFile = 0;

    //! Samples waiting to be written to the current file
    U8 m_writeBuffer[WRITE_BUFFER_SIZE];
    U32 m_bufferedSamples = 0;

    //! Time (seconds) when the current file received its first sample
    U32 m_fileStartSeconds = 0;

    //! Whether samples are recorded to the filesystem
    bool m_recording = false;

    //! Counters for telemetry
    U32 m_samplesRecorded = 0;
    U32 m_filesWritten = 0;
    U32 m_parseErrors = 0;
    U32 m_filesystemErrors = 0;
};

}  // namespace Components

#endif
