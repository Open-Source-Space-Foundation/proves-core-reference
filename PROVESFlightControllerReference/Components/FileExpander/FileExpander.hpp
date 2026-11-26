// ======================================================================
// \title  FileExpander.hpp
// \brief  Rebuilds UPZ-compressed uplinked files in bounded rate-group steps
// ======================================================================

#ifndef Components_FileExpander_HPP
#define Components_FileExpander_HPP

#include "FprimeExtras/Update/Delta/DeltaMedia.hpp"
#include "PROVESFlightControllerReference/Components/FileExpander/FileExpanderComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/FileExpander/UpzExpander.hpp"

namespace Components {

class FileExpander final : public FileExpanderComponentBase {
  public:
    //! Decoded bytes allowed per tick
    static constexpr U32 OUT_BYTES_PER_STEP = 4096;
    //! Storage calls allowed per tick (reads, writes, flushes)
    static constexpr U32 IO_PER_STEP = 64;
    //! Queued messages dispatched per tick
    static constexpr FwSizeType MAX_DISPATCH_PER_TICK = 4;

    explicit FileExpander(const char* const compName);
    ~FileExpander();

  private:
    //! Upz::Io over two DeltaFileMedia: the coded file (read only) and the output (read/write)
    class MediaIo final : public Upz::Io {
      public:
        bool open(const char* coded, const char* out);
        void close();
        bool codedSize(uint64_t& size) override;
        bool readCoded(uint64_t offset, uint8_t* buffer, uint32_t length) override;
        bool outSize(uint64_t& size) override;
        bool readOut(uint64_t offset, uint8_t* buffer, uint32_t length) override;
        bool writeOut(uint64_t offset, const uint8_t* buffer, uint32_t length) override;
        bool flushOut() override;
        bool resetOut() override;

      private:
        Update::DeltaFileMedia m_coded;
        Update::DeltaFileMedia m_out;
        Fw::FileNameString m_outPath;
    };

    void run_handler(FwIndexType portNum, U32 context) override;
    void EXPAND_FILE_cmdHandler(FwOpcodeType opCode,
                                U32 cmdSeq,
                                const Fw::CmdStringArg& coded_file,
                                const Fw::CmdStringArg& out_file) override;
    void ABORT_EXPAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    void finish();
    bool busy() const;

    Upz::Expander m_expander;
    MediaIo m_io;
    Fw::FileNameString m_outFile;
    Upz::Phase m_lastPhase;
    U32 m_ticks;
};

}  // namespace Components

#endif
