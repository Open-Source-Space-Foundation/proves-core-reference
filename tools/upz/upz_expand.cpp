// Host harness for Upz::Expander: expands a UPZ file into an output file with the
// same bounded steps the flight component uses. --stop-after N quits after N
// steps (a simulated reset) so a rerun exercises resume.
//
//   upz_expand IN OUT [--stop-after N] [--out-budget B] [--io-budget I] [--flip OFFSET]
// Prints one line: phase status steps farReads flushes resumeChunk
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "PROVESFlightControllerReference/Components/FileExpander/UpzExpander.hpp"

namespace {

class FileIo final : public Upz::Io {
  public:
    FileIo(const char* in, const char* out) : m_outPath(out) {
        m_in = std::fopen(in, "rb");
        m_out = std::fopen(out, "r+b");
        if (m_out == nullptr) {
            m_out = std::fopen(out, "w+b");
        }
    }
    ~FileIo() override {
        if (m_in)
            std::fclose(m_in);
        if (m_out)
            std::fclose(m_out);
    }
    bool ok() const { return m_in != nullptr && m_out != nullptr; }
    long flipOffset = -1;  // corrupt one decoded byte once, to exercise the chunk retry

    bool codedSize(uint64_t& size) override { return fileSize(m_in, size); }
    bool readCoded(uint64_t offset, uint8_t* buffer, uint32_t length) override {
        return std::fseek(m_in, static_cast<long>(offset), SEEK_SET) == 0 &&
               std::fread(buffer, 1, length, m_in) == length;
    }
    bool outSize(uint64_t& size) override { return fileSize(m_out, size); }
    bool readOut(uint64_t offset, uint8_t* buffer, uint32_t length) override {
        return std::fseek(m_out, static_cast<long>(offset), SEEK_SET) == 0 &&
               std::fread(buffer, 1, length, m_out) == length;
    }
    bool writeOut(uint64_t offset, const uint8_t* buffer, uint32_t length) override {
        uint8_t copy[Upz::BLOCK_SIZE];
        std::memcpy(copy, buffer, length);
        if (flipOffset >= 0 && static_cast<uint64_t>(flipOffset) >= offset &&
            static_cast<uint64_t>(flipOffset) < offset + length) {
            copy[flipOffset - offset] ^= 0x01;
            flipOffset = -1;
        }
        return std::fseek(m_out, static_cast<long>(offset), SEEK_SET) == 0 &&
               std::fwrite(copy, 1, length, m_out) == length;
    }
    bool flushOut() override { return std::fflush(m_out) == 0; }
    bool resetOut() override {
        std::fclose(m_out);
        m_out = std::fopen(m_outPath.c_str(), "w+b");
        return m_out != nullptr;
    }

  private:
    static bool fileSize(FILE* f, uint64_t& size) {
        if (std::fseek(f, 0, SEEK_END) != 0)
            return false;
        const long s = std::ftell(f);
        if (s < 0)
            return false;
        size = static_cast<uint64_t>(s);
        return true;
    }
    FILE* m_in = nullptr;
    FILE* m_out = nullptr;
    std::string m_outPath;
};

Upz::Expander g_expander;  // ~11.7 KB: keep it off the stack, as the component does

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s IN OUT [--stop-after N] [--out-budget B] [--io-budget I] [--flip OFF]\n",
                     argv[0]);
        return 2;
    }
    long stopAfter = -1;
    uint32_t outBudget = 4096;
    uint32_t ioBudget = 64;
    long flip = -1;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--stop-after"))
            stopAfter = std::atol(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--out-budget"))
            outBudget = static_cast<uint32_t>(std::atol(argv[i + 1]));
        else if (!std::strcmp(argv[i], "--io-budget"))
            ioBudget = static_cast<uint32_t>(std::atol(argv[i + 1]));
        else if (!std::strcmp(argv[i], "--flip"))
            flip = std::atol(argv[i + 1]);
    }
    FileIo io(argv[1], argv[2]);
    if (!io.ok()) {
        std::fprintf(stderr, "cannot open files\n");
        return 2;
    }
    io.flipOffset = flip;
    g_expander.begin(io);
    long steps = 0;
    Upz::Phase phase = g_expander.phase();
    while (phase != Upz::Phase::DONE && phase != Upz::Phase::FAILED) {
        if (stopAfter >= 0 && steps >= stopAfter)
            break;
        if (steps >= 50000000L) {
            std::fprintf(stderr, "no progress: step cap reached\n");
            return 4;
        }
        phase = g_expander.step(outBudget, ioBudget);
        steps++;
    }
    std::printf("phase=%u status=%u steps=%ld farReads=%u flushes=%u resumeChunk=%u sizeof=%zu\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(g_expander.status()), steps, g_expander.farReads(),
                g_expander.flushes(), g_expander.resumeChunk(), sizeof(Upz::Expander));
    if (phase == Upz::Phase::DONE)
        return 0;
    if (phase == Upz::Phase::FAILED)
        return 1;
    return 3;  // stopped early
}
