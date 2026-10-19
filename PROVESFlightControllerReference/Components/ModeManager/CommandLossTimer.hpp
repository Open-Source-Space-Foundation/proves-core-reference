// ======================================================================
// \title  CommandLossTimer.hpp
// \brief  Pure command-loss dead-man logic: counting, persistence cadence, and resume
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {

//! Seconds without an authenticated packet, counted on the 1 Hz tick.
//!
//! The count is uptime based, so it does not depend on the RTC. The caller
//! persists it whenever tick() asks, and feeds the stored value back through
//! resume() on boot, so a reboot loses at most PERSIST_PERIOD_S of progress.
//! reset() only touches RAM; the write that records a reset happens on the
//! next tick, which keeps flash writes off the packet path.
class CommandLossTimer {
  public:
    static constexpr std::uint32_t PERSIST_PERIOD_S = 60;
    static constexpr std::size_t RECORD_SIZE = 8;

    struct TickResult {
        bool persist;                //!< Caller should write persistValue now
        std::uint32_t persistValue;  //!< Elapsed seconds to store
        bool expired;                //!< Command loss limit reached this tick
        std::uint32_t lossSeconds;   //!< Elapsed seconds at expiry (valid when expired)
    };

    //! Continue counting from a value restored from storage
    void resume(std::uint32_t storedSeconds);

    //! An authenticated packet arrived
    void reset();

    //! Advance one second and check the limit
    //!
    //! On expiry the count restarts from zero, so a reboot after the
    //! command-loss power cycle waits a full limit again instead of looping.
    TickResult tick(std::uint32_t limitSeconds);

    std::uint32_t elapsed() const { return m_elapsed; }

    //! Serialize a stored value: little-endian value followed by its complement
    static void encode(std::uint32_t seconds, std::uint8_t (&record)[RECORD_SIZE]);

    //! Parse a stored record; false on short or corrupt input
    static bool decode(const std::uint8_t* record, std::size_t size, std::uint32_t& seconds);

  private:
    void restart();

    std::uint32_t m_elapsed = 0;
    std::uint32_t m_persisted = 0;  //!< Last value handed out for persisting
    bool m_resetPending = false;    //!< Stored value is stale after a reset
    bool m_expired = false;         //!< Latched until the next authenticated packet
};

}  // namespace Components
