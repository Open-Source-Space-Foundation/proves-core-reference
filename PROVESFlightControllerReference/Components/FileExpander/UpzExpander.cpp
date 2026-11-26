// ======================================================================
// \title  UpzExpander.cpp
// \brief  Resumable, allocation-free decoder for UPZ compressed files
//
// The LZMA decoding follows the public-domain reference decoder (LzmaSpec.cpp,
// Igor Pavlov), restructured so that every step is bounded and the match
// window is served from a RAM ring or read back from the output.
// ======================================================================

#include "PROVESFlightControllerReference/Components/FileExpander/UpzExpander.hpp"

#include <cstring>

namespace Upz {

namespace {

constexpr uint64_t NO_BLOCK = ~static_cast<uint64_t>(0);
constexpr uint32_t PROB_INIT = 1024;
constexpr uint32_t TOP = 1U << 24;
constexpr uint32_t MATCH_MIN_LEN = 2;
constexpr uint32_t END_POS_MODEL_INDEX = 14;
constexpr uint32_t NUM_STATES = 12;

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

const uint32_t CRC_NIBBLE[16] = {0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU, 0x76DC4190U, 0x6B6B51F4U,
                                 0x4DB26158U, 0x5005713CU, 0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
                                 0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU};

}  // namespace

uint32_t crc32Update(uint32_t crc, const uint8_t* data, uint32_t length) {
    crc = ~crc;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        crc = (crc >> 4) ^ CRC_NIBBLE[crc & 0x0F];
        crc = (crc >> 4) ^ CRC_NIBBLE[crc & 0x0F];
    }
    return ~crc;
}

void thumbFilter(uint8_t* data, uint32_t length, uint32_t base, bool encode) {
    uint32_t i = 0;
    while (i + 4 <= length) {
        const uint32_t h1 = static_cast<uint32_t>(data[i]) | (static_cast<uint32_t>(data[i + 1]) << 8);
        const uint32_t h2 = static_cast<uint32_t>(data[i + 2]) | (static_cast<uint32_t>(data[i + 3]) << 8);
        const bool isBl = (h2 & 0xD000U) == 0xD000U;
        const bool isBw = (h2 & 0xD000U) == 0x9000U;
        if ((h1 & 0xF800U) != 0xF000U || !(isBl || isBw)) {
            i += 2;
            continue;
        }
        // The opcode bits that select a site are never changed, so decode finds exactly the sites encode changed
        const uint32_t s = (h1 >> 10) & 1U;
        const uint32_t i1 = (~(((h2 >> 13) & 1U) ^ s)) & 1U;
        const uint32_t i2 = (~(((h2 >> 11) & 1U) ^ s)) & 1U;
        const uint32_t off = (s << 24) | (i1 << 23) | (i2 << 22) | ((h1 & 0x3FFU) << 12) | ((h2 & 0x7FFU) << 1);
        const uint32_t pc = base + i + 4;
        const uint32_t t = (encode ? (off + pc) : (off - pc)) & 0x1FFFFFFU;
        const uint32_t s2 = (t >> 24) & 1U;
        const uint32_t j1 = (~(((t >> 23) & 1U) ^ s2)) & 1U;
        const uint32_t j2 = (~(((t >> 22) & 1U) ^ s2)) & 1U;
        const uint32_t n1 = 0xF000U | (s2 << 10) | ((t >> 12) & 0x3FFU);
        const uint32_t n2 = (isBl ? 0xD000U : 0x9000U) | (j1 << 13) | (j2 << 11) | ((t >> 1) & 0x7FFU);
        data[i] = static_cast<uint8_t>(n1);
        data[i + 1] = static_cast<uint8_t>(n1 >> 8);
        data[i + 2] = static_cast<uint8_t>(n2);
        data[i + 3] = static_cast<uint8_t>(n2 >> 8);
        i += 4;
    }
}

Expander::Expander()
    : m_io(nullptr),
      m_phase(Phase::IDLE),
      m_status(Status::OK),
      m_filter(0),
      m_lc(0),
      m_lp(0),
      m_pb(0),
      m_chunkLog2(0),
      m_dictSize(0),
      m_rawSize(0),
      m_rawCrc(0),
      m_chunkCount(0),
      m_codedSize(0),
      m_dataStart(0),
      m_headerPos(0),
      m_crc(0),
      m_chunk(0),
      m_resumeChunk(0),
      m_retries(0),
      m_finalRetried(false),
      m_outPos(0),
      m_chunkEndPos(0),
      m_chunkCrcExpected(0),
      m_scanPos(0),
      m_ringValidFrom(0),
      m_writtenTo(0),
      m_flushedTo(0),
      m_ioUsed(0),
      m_farReads(0),
      m_flushes(0),
      m_range(0),
      m_code(0),
      m_state(0),
      m_rep{0, 0, 0, 0},
      m_pendingLen(0),
      m_codedPos(0),
      m_codedEnd(0),
      m_inPos(0),
      m_inLen(0),
      m_inBase(0),
      m_model(),
      m_in(),
      m_ring(),
      m_block(),
      m_blockIndex(NO_BLOCK),
      m_stage() {}

void Expander::begin(Io& io) {
    m_io = &io;
    m_phase = Phase::HEADER;
    m_status = Status::OK;
    m_headerPos = 0;
    m_crc = 0;
    m_chunk = 0;
    m_resumeChunk = 0;
    m_retries = 0;
    m_finalRetried = false;
    m_outPos = 0;
    m_chunkEndPos = 0;
    m_codedPos = 0;
    m_codedEnd = 0;
    m_inPos = 0;
    m_inLen = 0;
    m_pendingLen = 0;
    m_state = 0;
    m_rep[0] = m_rep[1] = m_rep[2] = m_rep[3] = 0;
    m_writtenTo = 0;
    m_flushedTo = 0;
    m_ringValidFrom = 0;
    m_scanPos = 0;
    m_farReads = 0;
    m_flushes = 0;
    m_blockIndex = NO_BLOCK;
}

void Expander::abort() {
    m_phase = Phase::IDLE;
    m_io = nullptr;
}

void Expander::fail(Status status) {
    m_status = status;
    m_phase = Phase::FAILED;
}

Phase Expander::step(uint32_t maxOutBytes, uint32_t maxIo) {
    if (m_io == nullptr) {
        return m_phase;
    }
    m_ioUsed = 0;
    bool ok = true;
    switch (m_phase) {
        case Phase::HEADER:
            ok = stepHeader(maxIo);
            break;
        case Phase::RESUME:
            ok = stepResume(maxIo);
            break;
        case Phase::DECODE:
            ok = stepDecode(maxOutBytes, maxIo);
            break;
        case Phase::FINAL:
            ok = stepFinal(maxIo);
            break;
        default:
            break;
    }
    if (!ok && m_phase != Phase::FAILED) {
        fail(Status::IO_ERROR);
    }
    return m_phase;
}

// ----------------------------------------------------------------------
// HEADER: parse the fixed header, then CRC the header and the chunk table
// ----------------------------------------------------------------------

bool Expander::stepHeader(uint32_t maxIo) {
    if (m_headerPos == 0) {
        uint8_t* h = m_block;
        m_ioUsed++;
        if (!m_io->readCoded(0, h, HEADER_FIXED_SIZE)) {
            fail(Status::BAD_HEADER);
            return true;
        }
        if (le32(h) != MAGIC || h[4] != VERSION || h[10] != 0 || h[11] != 0) {
            fail(Status::BAD_HEADER);
            return true;
        }
        m_filter = h[5];
        m_lc = h[6];
        m_lp = h[7];
        m_pb = h[8];
        m_chunkLog2 = h[9];
        m_dictSize = le32(h + 12);
        m_rawSize = le32(h + 16);
        m_rawCrc = le32(h + 20);
        m_chunkCount = le32(h + 24);
        m_codedSize = le32(h + 28);
        if (m_filter > 1 || m_lc + m_lp > 1 || m_pb > 4 || m_chunkLog2 < MIN_CHUNK_LOG2 ||
            m_chunkLog2 > MAX_CHUNK_LOG2 || m_dictSize == 0) {
            fail(Status::UNSUPPORTED);
            return true;
        }
        const uint64_t expectedChunks = (static_cast<uint64_t>(m_rawSize) + (1ULL << m_chunkLog2) - 1) >> m_chunkLog2;
        if (m_chunkCount > MAX_CHUNK_COUNT || m_chunkCount != expectedChunks) {
            fail(Status::BAD_HEADER);
            return true;
        }
        m_dataStart = HEADER_FIXED_SIZE + 8 * m_chunkCount + 4;
        uint64_t size = 0;
        m_ioUsed++;
        if (!m_io->codedSize(size)) {
            return false;
        }
        if (size != static_cast<uint64_t>(m_dataStart) + m_codedSize) {
            fail(Status::BAD_HEADER);
            return true;
        }
        m_crc = crc32Update(0, h, HEADER_FIXED_SIZE);
        m_headerPos = HEADER_FIXED_SIZE;
    }
    const uint32_t tableEnd = m_dataStart - 4;
    for (bool first = true; m_headerPos < tableEnd && (first || m_ioUsed < maxIo); first = false) {
        uint32_t n = tableEnd - m_headerPos;
        n = n > BLOCK_SIZE ? BLOCK_SIZE : n;
        m_ioUsed++;
        if (!m_io->readCoded(m_headerPos, m_block, n)) {
            return false;
        }
        m_crc = crc32Update(m_crc, m_block, n);
        m_headerPos += n;
    }
    if (m_headerPos < tableEnd) {
        return true;
    }
    m_ioUsed++;
    if (!m_io->readCoded(tableEnd, m_block, 4)) {
        return false;
    }
    if (le32(m_block) != m_crc) {
        fail(Status::BAD_HEADER_CRC);
        return true;
    }
    uint64_t outSize = 0;
    m_ioUsed++;
    if (!m_io->outSize(outSize)) {
        return false;
    }
    if (outSize > m_rawSize) {
        // Not a prefix of this output: start over
        m_ioUsed++;
        if (!m_io->resetOut()) {
            return false;
        }
    }
    m_phase = Phase::RESUME;
    m_chunk = 0;
    m_scanPos = 0;
    m_crc = 0;
    return true;
}

// ----------------------------------------------------------------------
// RESUME: keep every leading chunk whose bytes already match the table
// ----------------------------------------------------------------------

bool Expander::readTableEntry(uint32_t index, uint32_t& codedEnd, uint32_t& rawCrc) {
    uint8_t e[8];
    m_ioUsed++;
    if (!m_io->readCoded(HEADER_FIXED_SIZE + 8ULL * index, e, 8)) {
        return false;
    }
    codedEnd = le32(e);
    rawCrc = le32(e + 4);
    return true;
}

uint64_t Expander::chunkStart(uint32_t index) const {
    return static_cast<uint64_t>(index) << m_chunkLog2;
}

uint64_t Expander::chunkEnd(uint32_t index) const {
    const uint64_t end = static_cast<uint64_t>(index + 1) << m_chunkLog2;
    return end > m_rawSize ? m_rawSize : end;
}

bool Expander::stepResume(uint32_t maxIo) {
    uint64_t outSize = 0;
    m_ioUsed++;
    if (!m_io->outSize(outSize)) {
        return false;
    }
    // At least one pass per step, so any budget makes progress
    for (bool first = true; first || m_ioUsed < maxIo; first = false) {
        if (m_chunk >= m_chunkCount) {
            // Everything is present: verify the whole file
            m_phase = Phase::FINAL;
            m_scanPos = 0;
            m_crc = 0;
            return true;
        }
        const uint64_t end = chunkEnd(m_chunk);
        bool good = outSize >= end;
        if (good && m_scanPos < end) {
            uint32_t n = static_cast<uint32_t>(end - m_scanPos);
            n = n > BLOCK_SIZE ? BLOCK_SIZE : n;
            m_ioUsed++;
            if (!m_io->readOut(m_scanPos, m_block, n)) {
                return false;
            }
            m_crc = crc32Update(m_crc, m_block, n);
            m_scanPos += n;
            continue;
        }
        if (good) {
            uint32_t codedEnd = 0;
            uint32_t expected = 0;
            if (!readTableEntry(m_chunk, codedEnd, expected)) {
                return false;
            }
            good = m_crc == expected;
        }
        if (good) {
            m_chunk++;
            m_crc = 0;
            continue;
        }
        // First chunk that is missing or wrong: decode from here
        m_resumeChunk = m_chunk;
        if (m_chunk == 0 && outSize > 0) {
            m_ioUsed++;
            if (!m_io->resetOut()) {
                return false;
            }
        }
        m_writtenTo = chunkStart(m_chunk);
        m_flushedTo = m_writtenTo;
        m_ringValidFrom = m_writtenTo;
        m_blockIndex = NO_BLOCK;
        if (!startChunk(m_chunk)) {
            return false;
        }
        if (m_phase != Phase::FAILED) {
            m_phase = Phase::DECODE;
        }
        return true;
    }
    return true;
}

// ----------------------------------------------------------------------
// DECODE
// ----------------------------------------------------------------------

bool Expander::startChunk(uint32_t index) {
    uint32_t codedStart = 0;
    uint32_t codedEnd = 0;
    uint32_t unused = 0;
    if (index > 0 && !readTableEntry(index - 1, codedStart, unused)) {
        return false;
    }
    if (!readTableEntry(index, codedEnd, m_chunkCrcExpected)) {
        return false;
    }
    if (codedEnd < codedStart || codedEnd > m_codedSize) {
        fail(Status::BAD_HEADER);
        return true;
    }
    m_chunk = index;
    m_outPos = chunkStart(index);
    m_chunkEndPos = chunkEnd(index);
    m_codedPos = static_cast<uint64_t>(m_dataStart) + codedStart;
    m_codedEnd = static_cast<uint64_t>(m_dataStart) + codedEnd;
    m_inPos = 0;
    m_inLen = 0;
    m_crc = 0;
    m_pendingLen = 0;
    m_state = 0;
    m_rep[0] = m_rep[1] = m_rep[2] = m_rep[3] = 0;
    Prob* probs = reinterpret_cast<Prob*>(&m_model);
    for (size_t i = 0; i < sizeof(Model) / sizeof(Prob); i++) {
        probs[i] = PROB_INIT;
    }
    // Range decoder init: a zero byte, then the 32-bit code
    m_range = 0xFFFFFFFFU;
    m_code = 0;
    uint8_t b = 0;
    if (!nextByte(b)) {
        return m_phase == Phase::FAILED;
    }
    if (b != 0) {
        fail(Status::CORRUPT_STREAM);
        return true;
    }
    for (int i = 0; i < 4; i++) {
        if (!nextByte(b)) {
            return m_phase == Phase::FAILED;
        }
        m_code = (m_code << 8) | b;
    }
    if (m_code == m_range) {
        fail(Status::CORRUPT_STREAM);
    }
    return true;
}

bool Expander::nextByte(uint8_t& byte) {
    if (m_inPos == m_inLen) {
        if (m_codedPos >= m_codedEnd) {
            fail(Status::CORRUPT_STREAM);
            return false;
        }
        uint64_t n = m_codedEnd - m_codedPos;
        n = n > sizeof(m_in) ? sizeof(m_in) : n;
        m_ioUsed++;
        if (!m_io->readCoded(m_codedPos, m_in, static_cast<uint32_t>(n))) {
            fail(Status::IO_ERROR);
            return false;
        }
        m_codedPos += n;
        m_inPos = 0;
        m_inLen = static_cast<uint32_t>(n);
    }
    byte = m_in[m_inPos++];
    return true;
}

bool Expander::normalize() {
    if (m_range < TOP) {
        uint8_t b = 0;
        if (!nextByte(b)) {
            return false;
        }
        m_range <<= 8;
        m_code = (m_code << 8) | b;
    }
    return true;
}

bool Expander::bit(Prob& prob, uint32_t& out) {
    const uint32_t bound = (m_range >> 11) * prob;
    if (m_code < bound) {
        prob = static_cast<Prob>(prob + ((2048U - prob) >> 5));
        m_range = bound;
        out = 0;
    } else {
        prob = static_cast<Prob>(prob - (prob >> 5));
        m_code -= bound;
        m_range -= bound;
        out = 1;
    }
    return normalize();
}

bool Expander::directBits(uint32_t count, uint32_t& out) {
    uint32_t res = 0;
    for (uint32_t i = 0; i < count; i++) {
        m_range >>= 1;
        m_code -= m_range;
        const uint32_t t = 0U - (m_code >> 31);
        m_code += m_range & t;
        if (m_code == m_range) {
            fail(Status::CORRUPT_STREAM);
            return false;
        }
        if (!normalize()) {
            return false;
        }
        res = (res << 1) + (t + 1);
    }
    out = res;
    return true;
}

bool Expander::bitTree(Prob* probs, uint32_t numBits, uint32_t& out) {
    uint32_t m = 1;
    for (uint32_t i = 0; i < numBits; i++) {
        uint32_t b = 0;
        if (!bit(probs[m], b)) {
            return false;
        }
        m = (m << 1) + b;
    }
    out = m - (1U << numBits);
    return true;
}

bool Expander::bitTreeReverse(Prob* probs, uint32_t numBits, uint32_t& out) {
    uint32_t m = 1;
    uint32_t sym = 0;
    for (uint32_t i = 0; i < numBits; i++) {
        uint32_t b = 0;
        if (!bit(probs[m], b)) {
            return false;
        }
        m = (m << 1) + b;
        sym |= b << i;
    }
    out = sym;
    return true;
}

bool Expander::decodeLen(LenProbs& lp, uint32_t posState, uint32_t& len) {
    uint32_t b = 0;
    uint32_t v = 0;
    if (!bit(lp.choice, b)) {
        return false;
    }
    if (b == 0) {
        if (!bitTree(&lp.low[posState][0], 3, v)) {
            return false;
        }
        len = v;
        return true;
    }
    if (!bit(lp.choice2, b)) {
        return false;
    }
    if (b == 0) {
        if (!bitTree(&lp.mid[posState][0], 3, v)) {
            return false;
        }
        len = 8 + v;
        return true;
    }
    if (!bitTree(lp.high, 8, v)) {
        return false;
    }
    len = 16 + v;
    return true;
}

bool Expander::getByte(uint64_t position, uint8_t& byte) {
    if (position >= m_ringValidFrom && m_outPos - position <= RING_SIZE) {
        byte = m_ring[position & (RING_SIZE - 1)];
        return true;
    }
    // Older than the ring: the whole block is in the output; re-filter it as the encoder saw it
    const uint64_t index = position / BLOCK_SIZE;
    if (index != m_blockIndex) {
        const uint64_t start = index * BLOCK_SIZE;
        if (start >= m_rawSize) {
            fail(Status::CORRUPT_STREAM);
            return false;
        }
        const uint64_t end = (start + BLOCK_SIZE) > m_rawSize ? m_rawSize : (start + BLOCK_SIZE);
        if (end > m_flushedTo) {
            m_ioUsed++;
            m_flushes++;
            if (!m_io->flushOut()) {
                return false;
            }
            m_flushedTo = m_writtenTo;
        }
        m_ioUsed++;
        m_farReads++;
        if (!m_io->readOut(start, m_block, static_cast<uint32_t>(end - start))) {
            m_blockIndex = NO_BLOCK;
            return false;
        }
        if (m_filter == 1) {
            thumbFilter(m_block, static_cast<uint32_t>(end - start), static_cast<uint32_t>(start), true);
        }
        m_blockIndex = index;
    }
    byte = m_block[position % BLOCK_SIZE];
    return true;
}

bool Expander::putByte(uint8_t byte) {
    m_ring[m_outPos & (RING_SIZE - 1)] = byte;
    m_outPos++;
    if ((m_outPos % BLOCK_SIZE) != 0 && m_outPos != m_chunkEndPos) {
        return true;
    }
    const uint64_t start = (m_outPos - 1) & ~static_cast<uint64_t>(BLOCK_SIZE - 1);
    const uint32_t len = static_cast<uint32_t>(m_outPos - start);
    std::memcpy(m_stage, &m_ring[start & (RING_SIZE - 1)], len);
    if (m_filter == 1) {
        thumbFilter(m_stage, len, static_cast<uint32_t>(start), false);
    }
    m_crc = crc32Update(m_crc, m_stage, len);
    m_ioUsed++;
    if (!m_io->writeOut(start, m_stage, len)) {
        return false;
    }
    m_writtenTo = m_outPos;
    return true;
}

bool Expander::decodeSymbol() {
    const uint32_t posState = static_cast<uint32_t>(m_outPos) & ((1U << m_pb) - 1);
    const uint64_t dictLimit = m_outPos < m_dictSize ? m_outPos : m_dictSize;
    uint32_t b = 0;
    if (!bit(m_model.isMatch[(m_state << 4) + posState], b)) {
        return false;
    }
    if (b == 0) {
        uint32_t litState = (static_cast<uint32_t>(m_outPos) & ((1U << m_lp) - 1)) << m_lc;
        if (m_lc > 0 && m_outPos > 0) {
            uint8_t prev = 0;
            if (!getByte(m_outPos - 1, prev)) {
                return false;
            }
            litState += prev >> (8 - m_lc);
        }
        Prob* probs = &m_model.literal[0x300 * litState];
        uint32_t sym = 1;
        if (m_state >= 7) {
            if (static_cast<uint64_t>(m_rep[0]) >= dictLimit) {
                fail(Status::CORRUPT_STREAM);
                return false;
            }
            uint8_t mb = 0;
            if (!getByte(m_outPos - m_rep[0] - 1, mb)) {
                return false;
            }
            uint32_t matchByte = mb;
            do {
                const uint32_t matchBit = (matchByte >> 7) & 1;
                matchByte <<= 1;
                if (!bit(probs[((1 + matchBit) << 8) + sym], b)) {
                    return false;
                }
                sym = (sym << 1) | b;
                if (matchBit != b) {
                    break;
                }
            } while (sym < 0x100);
        }
        while (sym < 0x100) {
            if (!bit(probs[sym], b)) {
                return false;
            }
            sym = (sym << 1) | b;
        }
        m_state = m_state < 4 ? 0 : (m_state < 10 ? m_state - 3 : m_state - 6);
        return putByte(static_cast<uint8_t>(sym - 0x100));
    }

    uint32_t len = 0;
    if (!bit(m_model.isRep[m_state], b)) {
        return false;
    }
    if (b == 0) {
        m_rep[3] = m_rep[2];
        m_rep[2] = m_rep[1];
        m_rep[1] = m_rep[0];
        if (!decodeLen(m_model.len, posState, len)) {
            return false;
        }
        m_state = m_state < 7 ? 7 : 10;
        // Distance
        const uint32_t lenState = len < 3 ? len : 3;
        uint32_t posSlot = 0;
        if (!bitTree(m_model.posSlot[lenState], 6, posSlot)) {
            return false;
        }
        uint32_t dist = posSlot;
        if (posSlot >= 4) {
            const uint32_t numDirect = (posSlot >> 1) - 1;
            dist = (2 | (posSlot & 1)) << numDirect;
            uint32_t v = 0;
            if (posSlot < END_POS_MODEL_INDEX) {
                if (!bitTreeReverse(m_model.posDecoders + dist - posSlot, numDirect, v)) {
                    return false;
                }
                dist += v;
            } else {
                if (!directBits(numDirect - 4, v)) {
                    return false;
                }
                dist += v << 4;
                if (!bitTreeReverse(m_model.align, 4, v)) {
                    return false;
                }
                dist += v;
            }
        }
        // The end marker (dist 0xFFFFFFFF) is never reached: decoding stops at the chunk's known size
        m_rep[0] = dist;
    } else {
        if (!bit(m_model.isRepG0[m_state], b)) {
            return false;
        }
        if (b == 0) {
            if (!bit(m_model.isRep0Long[(m_state << 4) + posState], b)) {
                return false;
            }
            if (b == 0) {
                if (static_cast<uint64_t>(m_rep[0]) >= dictLimit) {
                    fail(Status::CORRUPT_STREAM);
                    return false;
                }
                m_state = m_state < 7 ? 9 : 11;
                uint8_t sb = 0;
                if (!getByte(m_outPos - m_rep[0] - 1, sb)) {
                    return false;
                }
                return putByte(sb);
            }
        } else {
            uint32_t dist = 0;
            if (!bit(m_model.isRepG1[m_state], b)) {
                return false;
            }
            if (b == 0) {
                dist = m_rep[1];
            } else {
                if (!bit(m_model.isRepG2[m_state], b)) {
                    return false;
                }
                if (b == 0) {
                    dist = m_rep[2];
                } else {
                    dist = m_rep[3];
                    m_rep[3] = m_rep[2];
                }
                m_rep[2] = m_rep[1];
            }
            m_rep[1] = m_rep[0];
            m_rep[0] = dist;
        }
        if (!decodeLen(m_model.repLen, posState, len)) {
            return false;
        }
        m_state = m_state < 7 ? 8 : 11;
    }
    len += MATCH_MIN_LEN;
    if (static_cast<uint64_t>(m_rep[0]) >= dictLimit || m_outPos + len > m_chunkEndPos) {
        fail(Status::CORRUPT_STREAM);
        return false;
    }
    m_pendingLen = len;
    return true;
}

bool Expander::stepDecode(uint32_t maxOutBytes, uint32_t maxIo) {
    const uint64_t startPos = m_outPos;
    while (m_outPos < m_chunkEndPos && (m_outPos - startPos) < maxOutBytes && m_ioUsed < maxIo) {
        if (m_pendingLen > 0) {
            uint8_t b = 0;
            if (!getByte(m_outPos - m_rep[0] - 1, b) || !putByte(b)) {
                return false;
            }
            m_pendingLen--;
            continue;
        }
        if (!decodeSymbol()) {
            return false;
        }
    }
    if (m_phase == Phase::DECODE && m_outPos == m_chunkEndPos && m_pendingLen == 0) {
        return finishChunk();
    }
    return true;
}

bool Expander::finishChunk() {
    m_ioUsed++;
    m_flushes++;
    if (!m_io->flushOut()) {
        return false;
    }
    m_flushedTo = m_writtenTo;
    if (m_crc != m_chunkCrcExpected) {
        // A one-off upset is redone once; a second mismatch means the stream or the decoder is wrong
        if (m_retries > 0) {
            // Still wrong: the window it matched into may be bad on storage, so rescan every stored chunk once
            m_retries = 0;
            return rescanOrFail(Status::CHUNK_CRC);
        }
        m_retries++;
        m_ringValidFrom = chunkStart(m_chunk);
        m_writtenTo = m_ringValidFrom;
        m_flushedTo = m_ringValidFrom;
        m_blockIndex = NO_BLOCK;
        return startChunk(m_chunk);
    }
    m_retries = 0;
    if (m_chunk + 1 >= m_chunkCount) {
        m_phase = Phase::FINAL;
        m_scanPos = 0;
        m_crc = 0;
        return true;
    }
    return startChunk(m_chunk + 1);
}

// ----------------------------------------------------------------------
// FINAL: the output on storage must be exactly raw_size bytes with raw_crc32
// ----------------------------------------------------------------------

bool Expander::rescanOrFail(Status status) {
    // Storage disagrees with what was decoded: rescan once, which redoes every chunk whose stored bytes are wrong
    if (m_finalRetried) {
        fail(status);
        return true;
    }
    m_finalRetried = true;
    m_phase = Phase::RESUME;
    m_chunk = 0;
    m_scanPos = 0;
    m_crc = 0;
    return true;
}

bool Expander::stepFinal(uint32_t maxIo) {
    if (m_scanPos == 0) {
        uint64_t size = 0;
        m_ioUsed++;
        if (!m_io->flushOut() || !m_io->outSize(size)) {
            return false;
        }
        if (size != m_rawSize) {
            return rescanOrFail(Status::FINAL_CRC);
        }
    }
    for (bool first = true; m_scanPos < m_rawSize && (first || m_ioUsed < maxIo); first = false) {
        uint64_t n = m_rawSize - m_scanPos;
        n = n > BLOCK_SIZE ? BLOCK_SIZE : n;
        m_ioUsed++;
        if (!m_io->readOut(m_scanPos, m_block, static_cast<uint32_t>(n))) {
            return false;
        }
        m_crc = crc32Update(m_crc, m_block, static_cast<uint32_t>(n));
        m_scanPos += n;
    }
    m_blockIndex = NO_BLOCK;
    if (m_scanPos < m_rawSize) {
        return true;
    }
    if (m_crc != m_rawCrc) {
        return rescanOrFail(Status::FINAL_CRC);
    }
    m_outPos = m_rawSize;
    m_phase = Phase::DONE;
    return true;
}

}  // namespace Upz
