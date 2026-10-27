// ======================================================================
// \title  SafeModePolicy.cpp
// \brief  cpp file for SafeModePolicy class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ModeManager/SafeModePolicy.hpp"

namespace Components {

namespace {

constexpr std::size_t MODE_OFFSET = 0;
constexpr std::size_t COUNT_OFFSET = 4;
constexpr std::size_t REASON_OFFSET = 8;
constexpr std::size_t CLEAN_SHUTDOWN_OFFSET = 9;

}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SafeModePolicy ::SafeModePolicy()
    : m_safeModeVoltageCounter(0), m_recoveryVoltageCounter(0), m_commandLossCounter(0), m_commandLossDebounce(false) {}

SafeModePolicy ::~SafeModePolicy() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

SafeModePolicy::Action SafeModePolicy ::tick(float voltage,
                                             bool valid,
                                             const Params& params,
                                             Mode mode,
                                             Reason reason) {
    Action action = Action::NONE;

    if (mode == Mode::NORMAL) {
        const bool isFault = !valid || (voltage < params.entryVoltage);

        if (isFault) {
            this->m_safeModeVoltageCounter++;
            if (this->m_safeModeVoltageCounter >= params.debounceSeconds) {
                action = Action::ENTER_LOW_BATTERY;
                this->m_safeModeVoltageCounter = 0;
            }
        } else {
            this->m_safeModeVoltageCounter = 0;
        }

        this->m_recoveryVoltageCounter = 0;
    } else if (mode == Mode::SAFE_MODE) {
        // Only a low battery recovers on its own. Every other reason needs the EXIT_SAFE_MODE command.
        if (reason == Reason::LOW_BATTERY) {
            if (valid && voltage > params.recoveryVoltage) {
                this->m_recoveryVoltageCounter++;
                if (this->m_recoveryVoltageCounter >= params.debounceSeconds) {
                    action = Action::EXIT_AUTO;
                    this->m_recoveryVoltageCounter = 0;
                }
            } else {
                this->m_recoveryVoltageCounter = 0;
            }
        }

        this->m_safeModeVoltageCounter = 0;
    }

    return action;
}

void SafeModePolicy ::commandLossTick() {
    this->m_commandLossCounter++;
}

void SafeModePolicy ::packetReceived() {
    this->m_commandLossCounter = 0;
    this->m_commandLossDebounce = false;
}

SafeModePolicy::Action SafeModePolicy ::commandLossCheck(std::uint32_t timeoutSeconds) {
    if (this->m_commandLossCounter >= timeoutSeconds && !this->m_commandLossDebounce) {
        this->m_commandLossDebounce = true;
        return Action::ENTER_COMMAND_LOSS;
    }
    return Action::NONE;
}

std::uint32_t SafeModePolicy ::commandLossSeconds() const {
    return this->m_commandLossCounter;
}

SafeModePolicy::StateBytes SafeModePolicy ::encodeState(Mode mode,
                                                        std::uint32_t safeModeEntryCount,
                                                        Reason reason,
                                                        bool cleanShutdown) {
    StateBytes bytes{};  // zero-initialized, so the padding bytes are 0
    bytes[MODE_OFFSET] = static_cast<std::uint8_t>(mode);
    for (std::size_t i = 0; i < 4; i++) {
        bytes[COUNT_OFFSET + i] = static_cast<std::uint8_t>((safeModeEntryCount >> (8 * i)) & 0xFFU);
    }
    bytes[REASON_OFFSET] = static_cast<std::uint8_t>(reason);
    bytes[CLEAN_SHUTDOWN_OFFSET] = cleanShutdown ? 1 : 0;
    return bytes;
}

SafeModePolicy::DecodedState SafeModePolicy ::decodeState(const StateBytes& bytes) {
    DecodedState decoded{};
    decoded.rawMode = bytes[MODE_OFFSET];

    if (decoded.rawMode < static_cast<std::uint8_t>(Mode::SAFE_MODE) ||
        decoded.rawMode > static_cast<std::uint8_t>(Mode::NORMAL)) {
        decoded.status = DecodedState::Status::CORRUPT_MODE;
        decoded.mode = Mode::NORMAL;
        decoded.safeModeEntryCount = 0;
        decoded.reason = Reason::NONE;
        decoded.unintendedReboot = false;
        return decoded;
    }

    std::uint32_t count = 0;
    for (std::size_t i = 0; i < 4; i++) {
        count |= static_cast<std::uint32_t>(bytes[COUNT_OFFSET + i]) << (8 * i);
    }

    decoded.status = DecodedState::Status::OK;
    decoded.mode = static_cast<Mode>(decoded.rawMode);
    decoded.safeModeEntryCount = count;
    decoded.reason = static_cast<Reason>(bytes[REASON_OFFSET]);
    decoded.unintendedReboot = (bytes[CLEAN_SHUTDOWN_OFFSET] == 0) && (decoded.mode == Mode::NORMAL);
    return decoded;
}

}  // namespace Components
