// ======================================================================
// \title  CommandLossTimer.cpp
// \brief  Pure command-loss dead-man logic: counting, persistence cadence, and resume
// ======================================================================

#include "CommandLossTimer.hpp"

#include <limits>

namespace Components {

void CommandLossTimer::resume(std::uint32_t storedSeconds) {
    m_elapsed = storedSeconds;
    m_persisted = storedSeconds;
    m_resetPending = false;
}

void CommandLossTimer::reset() {
    m_expired = false;
    restart();
}

void CommandLossTimer::restart() {
    m_elapsed = 0;
    m_resetPending = m_persisted != 0;
}

CommandLossTimer::TickResult CommandLossTimer::tick(std::uint32_t limitSeconds) {
    TickResult result{false, 0, false, 0};

    if (m_elapsed < std::numeric_limits<std::uint32_t>::max()) {
        m_elapsed++;
    }

    // m_persisted <= m_elapsed holds whenever no reset is pending
    if (m_resetPending) {
        m_persisted = 0;
        m_resetPending = false;
        result.persist = true;
        result.persistValue = 0;
    } else if (m_elapsed - m_persisted >= PERSIST_PERIOD_S) {
        m_persisted = m_elapsed;
        result.persist = true;
        result.persistValue = m_elapsed;
    }

    if (!m_expired && m_elapsed >= limitSeconds) {
        m_expired = true;
        result.expired = true;
        result.lossSeconds = m_elapsed;
        restart();
    }
    return result;
}

void CommandLossTimer::encode(std::uint32_t seconds, std::uint8_t (&record)[RECORD_SIZE]) {
    const std::uint32_t check = ~seconds;
    for (std::size_t i = 0; i < 4; i++) {
        record[i] = static_cast<std::uint8_t>(seconds >> (8 * i));
        record[4 + i] = static_cast<std::uint8_t>(check >> (8 * i));
    }
}

bool CommandLossTimer::decode(const std::uint8_t* record, std::size_t size, std::uint32_t& seconds) {
    if (record == nullptr || size != RECORD_SIZE) {
        return false;
    }
    std::uint32_t value = 0;
    std::uint32_t check = 0;
    for (std::size_t i = 0; i < 4; i++) {
        value |= static_cast<std::uint32_t>(record[i]) << (8 * i);
        check |= static_cast<std::uint32_t>(record[4 + i]) << (8 * i);
    }
    if (check != static_cast<std::uint32_t>(~value)) {
        return false;
    }
    seconds = value;
    return true;
}

}  // namespace Components
