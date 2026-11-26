// ======================================================================
// \title  UpzExpander.hpp
// \brief  Resumable, allocation-free decoder for UPZ compressed files
//
// A UPZ file is a raw-LZMA stream of a Thumb-2 branch-filtered input, cut into
// fixed-size chunks that each start from a fresh LZMA state but may match back
// into every earlier byte of the file. The decoder keeps only the last
// RING_SIZE bytes in RAM and reads older window bytes back from the output it
// is writing, so the compression window costs storage reads, not RAM.
//
// Container (little-endian), written by tools/upz/upz.py:
//   0  u32 magic "UPZ1"        4  u8 version (1)     5  u8 filter (0 none, 1 thumb2)
//   6  u8 lc  7 u8 lp  8 u8 pb 9  u8 chunk_log2      10 u16 reserved (0)
//   12 u32 dict_size           16 u32 raw_size       20 u32 raw_crc32
//   24 u32 chunk_count         28 u32 coded_size
//   32 chunk_count x { u32 coded_end, u32 raw_crc32 }   coded_end is relative to the data start
//   .. u32 header_crc32 over every byte before it, then the coded data
// CRC32 is the zlib/IEEE CRC32.
//
// No F Prime dependency: the engine is host-testable through the Io interface.
// ======================================================================
#ifndef Components_FileExpander_UpzExpander_HPP
#define Components_FileExpander_UpzExpander_HPP

#include <cstddef>
#include <cstdint>

namespace Upz {

constexpr uint32_t MAGIC = 0x315A5055U;  // "UPZ1"
constexpr uint8_t VERSION = 1;
constexpr uint32_t HEADER_FIXED_SIZE = 32;
constexpr uint32_t BLOCK_SIZE = 256;        // filter block and far-read cache unit
constexpr uint32_t RING_SIZE = 4096;        // recent output kept in RAM (power of two)
constexpr uint32_t MAX_CHUNK_COUNT = 1024;  // bounds the header CRC pass
constexpr uint8_t MIN_CHUNK_LOG2 = 12;
constexpr uint8_t MAX_CHUNK_LOG2 = 20;

//! Storage the expander reads and writes. All offsets are absolute.
class Io {
  public:
    virtual ~Io() = default;
    virtual bool codedSize(uint64_t& size) = 0;
    virtual bool readCoded(uint64_t offset, uint8_t* buffer, uint32_t length) = 0;
    virtual bool outSize(uint64_t& size) = 0;
    virtual bool readOut(uint64_t offset, uint8_t* buffer, uint32_t length) = 0;
    virtual bool writeOut(uint64_t offset, const uint8_t* buffer, uint32_t length) = 0;
    //! Make written bytes durable and visible to readOut
    virtual bool flushOut() = 0;
    //! Truncate the output to zero length
    virtual bool resetOut() = 0;
};

enum class Status : uint8_t {
    OK = 0,
    IO_ERROR = 1,
    BAD_HEADER = 2,
    BAD_HEADER_CRC = 3,
    UNSUPPORTED = 4,
    CORRUPT_STREAM = 5,
    CHUNK_CRC = 6,
    FINAL_CRC = 7,
};

enum class Phase : uint8_t {
    IDLE = 0,
    HEADER = 1,  //!< Reading and CRC-checking the header and chunk table
    RESUME = 2,  //!< Checking chunks already in the output against the table
    DECODE = 3,  //!< Decoding chunks
    FINAL = 4,   //!< Re-reading the whole output and checking raw_crc32
    DONE = 5,
    FAILED = 6,
};

uint32_t crc32Update(uint32_t crc, const uint8_t* data, uint32_t length);

//! Thumb-2 BL / B.W branch filter over one block whose first byte is at absolute offset `base`
void thumbFilter(uint8_t* data, uint32_t length, uint32_t base, bool encode);

class Expander {
  public:
    Expander();

    //! Start (or restart) expanding through `io`. Progress is derived from the output, never stored.
    void begin(Io& io);

    //! Drop the current job; the partial output is kept for a later resume
    void abort();

    //! Do one bounded unit of work: at most about `maxOutBytes` decoded bytes and `maxIo` storage calls
    Phase step(uint32_t maxOutBytes, uint32_t maxIo);

    Phase phase() const { return m_phase; }
    Status status() const { return m_status; }
    uint32_t chunkIndex() const { return m_chunk; }
    uint32_t chunkCount() const { return m_chunkCount; }
    uint32_t resumeChunk() const { return m_resumeChunk; }
    uint64_t outPosition() const { return m_outPos; }
    uint32_t rawSize() const { return m_rawSize; }
    uint32_t rawCrc() const { return m_rawCrc; }
    uint32_t farReads() const { return m_farReads; }
    uint32_t flushes() const { return m_flushes; }

  private:
    typedef uint16_t Prob;

    // Probability model for lc + lp <= 1 and pb <= 4: 3,383 entries, 6,766 B
    struct LenProbs {
        Prob choice;
        Prob choice2;
        Prob low[16][8];
        Prob mid[16][8];
        Prob high[256];
    };
    struct Model {
        Prob isMatch[12 * 16];
        Prob isRep[12];
        Prob isRepG0[12];
        Prob isRepG1[12];
        Prob isRepG2[12];
        Prob isRep0Long[12 * 16];
        Prob posSlot[4][64];
        Prob posDecoders[115];
        Prob align[16];
        LenProbs len;
        LenProbs repLen;
        Prob literal[0x300 * 2];
    };

    void fail(Status status);
    bool stepHeader(uint32_t maxIo);
    bool stepResume(uint32_t maxIo);
    bool stepDecode(uint32_t maxOutBytes, uint32_t maxIo);
    bool stepFinal(uint32_t maxIo);
    bool rescanOrFail(Status status);

    bool readTableEntry(uint32_t index, uint32_t& codedEnd, uint32_t& rawCrc);
    uint64_t chunkStart(uint32_t index) const;
    uint64_t chunkEnd(uint32_t index) const;
    bool startChunk(uint32_t index);
    bool finishChunk();

    // Range decoder
    bool nextByte(uint8_t& byte);
    bool normalize();
    bool bit(Prob& prob, uint32_t& out);
    bool directBits(uint32_t count, uint32_t& out);
    bool bitTree(Prob* probs, uint32_t numBits, uint32_t& out);
    bool bitTreeReverse(Prob* probs, uint32_t numBits, uint32_t& out);
    bool decodeLen(LenProbs& lp, uint32_t posState, uint32_t& len);
    bool decodeSymbol();

    // Window
    bool getByte(uint64_t position, uint8_t& byte);
    bool putByte(uint8_t byte);

    Io* m_io;
    Phase m_phase;
    Status m_status;

    // Header
    uint8_t m_filter;
    uint8_t m_lc;
    uint8_t m_lp;
    uint8_t m_pb;
    uint8_t m_chunkLog2;
    uint32_t m_dictSize;
    uint32_t m_rawSize;
    uint32_t m_rawCrc;
    uint32_t m_chunkCount;
    uint32_t m_codedSize;
    uint32_t m_dataStart;
    uint32_t m_headerPos;  //!< Header CRC pass progress
    uint32_t m_crc;        //!< Running CRC for the current pass

    // Progress
    uint32_t m_chunk;
    uint32_t m_resumeChunk;
    uint32_t m_retries;
    bool m_finalRetried;  //!< One storage rescan per job
    uint64_t m_outPos;
    uint64_t m_chunkEndPos;
    uint32_t m_chunkCrcExpected;
    uint64_t m_scanPos;  //!< RESUME / FINAL read position
    uint64_t m_ringValidFrom;
    uint64_t m_writtenTo;
    uint64_t m_flushedTo;
    uint32_t m_ioUsed;
    uint32_t m_farReads;
    uint32_t m_flushes;

    // LZMA state
    uint32_t m_range;
    uint32_t m_code;
    uint32_t m_state;
    uint32_t m_rep[4];
    uint32_t m_pendingLen;
    uint64_t m_codedPos;
    uint64_t m_codedEnd;
    uint32_t m_inPos;
    uint32_t m_inLen;
    uint64_t m_inBase;
    Model m_model;

    uint8_t m_in[64];
    uint8_t m_ring[RING_SIZE];
    uint8_t m_block[BLOCK_SIZE];  //!< Far-read cache and scratch for passes
    uint64_t m_blockIndex;        //!< Block held in m_block, or NO_BLOCK
    uint8_t m_stage[BLOCK_SIZE];  //!< Unfilter scratch for writes
};

}  // namespace Upz

#endif
