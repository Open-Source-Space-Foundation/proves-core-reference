// ======================================================================
// \title  SafeModePolicy.hpp
// \brief  hpp file for SafeModePolicy class
// ======================================================================

#pragma once

#include <array>
#include <cstdint>

namespace Components {

//! Safe mode decisions, free of F Prime and Zephyr types
//!
//! Holds the debounce counters for low-voltage entry, auto-recovery and command loss, and decodes the persisted
//! mode state. The component turns the returned Action into events, telemetry and port calls.
//!
//! Not thread safe. The voltage counters belong to the rate group thread. The command loss counters are touched
//! from the rate group thread and the packetRouted port, so the caller must serialize calls to
//! commandLossTick(), commandLossCheck() and packetReceived().
class SafeModePolicy {
  public:
    // ----------------------------------------------------------------------
    // Public types
    // ----------------------------------------------------------------------

    //! System mode, mirrors the private SystemMode enum of ModeManager
    enum class Mode : std::uint8_t { SAFE_MODE = 1, NORMAL = 2 };

    //! Safe mode reason, mirrors the FPP SafeModeReason enum
    enum class Reason : std::uint8_t {
        NONE = 0,
        LOW_BATTERY = 1,
        SYSTEM_FAULT = 2,
        GROUND_COMMAND = 3,
        EXTERNAL_REQUEST = 4,
        LORA = 5,
        COMMAND_LOSS = 6
    };

    //! What the component must do after a tick
    enum class Action { NONE, ENTER_LOW_BATTERY, EXIT_AUTO, ENTER_COMMAND_LOSS };

    //! Voltage thresholds and debounce, read from the component parameters on every tick
    struct Params {
        float entryVoltage;             //!< Enter safe mode below this voltage
        float recoveryVoltage;          //!< Leave safe mode above this voltage
        std::uint32_t debounceSeconds;  //!< Consecutive ticks a condition must hold
    };

    //! Bytes of the persisted mode state
    static constexpr std::size_t STATE_SIZE = 12;

    //! Persisted mode state, as stored in the file
    using StateBytes = std::array<std::uint8_t, STATE_SIZE>;

    //! Result of decoding the persisted mode state
    struct DecodedState {
        enum class Status { OK, CORRUPT_MODE };

        Status status;                     //!< OK, or CORRUPT_MODE if the mode byte is out of range
        std::uint8_t rawMode;              //!< The mode byte as stored, for reporting corruption
        Mode mode;                         //!< Restored mode, NORMAL if corrupt
        std::uint32_t safeModeEntryCount;  //!< Restored entry count, 0 if corrupt
        Reason reason;                     //!< Restored reason, NONE if corrupt
        bool unintendedReboot;             //!< Unclean shutdown while in NORMAL mode
    };

    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SafeModePolicy object
    SafeModePolicy();

    //! Destroy SafeModePolicy object
    ~SafeModePolicy();

    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Evaluate one voltage reading, once per second
    //!
    //! In NORMAL mode an invalid reading or a voltage below the entry threshold counts towards
    //! debounceSeconds and any good reading resets the count. In SAFE_MODE with reason LOW_BATTERY a valid
    //! voltage above the recovery threshold counts towards debounceSeconds. No other reason ever recovers
    //! automatically. A returned ENTER_LOW_BATTERY or EXIT_AUTO has already cleared its counter.
    Action tick(float voltage,         //!< The measured voltage in volts, ignored if not valid
                bool valid,            //!< Whether the voltage reading is valid
                const Params& params,  //!< The thresholds and debounce
                Mode mode,             //!< The current mode
                Reason reason          //!< The current safe mode reason
    );

    //! Count one second since the last packet
    void commandLossTick();

    //! Reset the command loss timer and re-arm it after a packet was routed
    void packetReceived();

    //! Check the command loss timeout
    //!
    //! Returns ENTER_COMMAND_LOSS at most once until packetReceived() re-arms it, NONE otherwise.
    Action commandLossCheck(std::uint32_t timeoutSeconds  //!< Seconds without a packet before command loss
    );

    //! Get the seconds since the last packet
    std::uint32_t commandLossSeconds() const;

    //! Encode the mode state into the persisted layout
    //!
    //! The layout is little-endian and matches the 12-byte struct earlier firmware wrote, so existing state
    //! files still decode: mode at 0, entry count at 4, reason at 8, clean shutdown flag at 9. Other bytes are 0.
    static StateBytes encodeState(Mode mode,                         //!< The mode to persist
                                  std::uint32_t safeModeEntryCount,  //!< The entry count to persist
                                  Reason reason,                     //!< The reason to persist
                                  bool cleanShutdown                 //!< Whether the shutdown was intended
    );

    //! Decode the persisted mode state
    //!
    //! A mode byte outside SAFE_MODE..NORMAL is corrupt and decodes to the defaults. The reason byte is not
    //! range checked. An unintended reboot is a clean shutdown flag of 0 while in NORMAL mode.
    static DecodedState decodeState(const StateBytes& bytes  //!< The persisted bytes
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::uint32_t m_safeModeVoltageCounter;  //!< Ticks of low voltage in NORMAL mode
    std::uint32_t m_recoveryVoltageCounter;  //!< Ticks of recovered voltage in SAFE_MODE
    std::uint32_t m_commandLossCounter;      //!< Seconds since the last packet
    bool m_commandLossDebounce;              //!< Set once command loss fired, until a packet re-arms it
};

}  // namespace Components
