// ======================================================================
// \title  DeploymentSequencer.hpp
// \brief  hpp file for DeploymentSequencer class
// ======================================================================

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Components {

//! Burn and retry sequencing for the antenna deployment, free of F Prime types
//!
//! IDLE -> BURNING -> RETRY_WAIT -> BURNING ... -> IDLE, advanced one scheduler tick at a time. The sequencer
//! never touches the burnwire or the deployed flag. Every call returns a Step listing, in order, what the
//! component has to do: drive the burnwire, log events and write the deployed flag.
class DeploymentSequencer {
  public:
    // ----------------------------------------------------------------------
    // Public types
    // ----------------------------------------------------------------------

    //! Deployment result, mirrors the FPP DeployResult enum
    enum class Result : std::uint8_t { SUCCESS = 0, ABORT = 1, FAILED = 2 };

    //! Things the component has to do
    enum class EffectKind : std::uint8_t {
        BURN_START,         //!< Start the burn
        BURN_STOP,          //!< Stop the burn
        ATTEMPT_STARTED,    //!< Log DeployAttempt(attempt) and write DeployAttemptCount(attemptCount)
        BURN_SIGNAL_COUNT,  //!< Log AntennaBurnSignalCount(burnTicks)
        MARK_DEPLOYED,      //!< Log DeploySuccess(attempt) and write the deployed flag
        FINISHED,           //!< Log DeployFinish(result, attempt)
    };

    //! One thing to do, with the values it needs
    struct Effect {
        EffectKind kind;
        std::uint32_t attempt;       //!< The attempt number, for ATTEMPT_STARTED, MARK_DEPLOYED and FINISHED
        std::uint32_t attemptCount;  //!< Attempts started before this one, for ATTEMPT_STARTED
        std::uint32_t burnTicks;     //!< Ticks the burn was held, for BURN_SIGNAL_COUNT
        Result result;               //!< The outcome, for FINISHED
    };

    //! The ordered effects of one call
    class Step {
      public:
        //! The most effects one call produces: stop, count, stop, deployed, finished
        static constexpr std::size_t MAX_EFFECTS = 6;

        Step() : m_count(0) {}

        //! Number of effects
        std::size_t size() const { return this->m_count; }

        //! The effect at the index, which must be below size()
        const Effect& operator[](std::size_t index) const { return this->m_effects[index]; }

        //! Append an effect. Returns false and drops it if the step is full, which the sequencer never does.
        bool add(const Effect& effect);

      private:
        std::array<Effect, MAX_EFFECTS> m_effects;
        std::size_t m_count;
    };

    //! Parameters, read from the component on every tick
    struct Params {
        std::uint32_t burnDurationSec;  //!< Ticks to hold the burn
        std::uint32_t retryDelaySec;    //!< Ticks to wait between attempts, 0 retries on the next tick
        std::uint32_t maxAttempts;      //!< Attempts before giving up
    };

    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DeploymentSequencer object
    DeploymentSequencer();

    //! Destroy DeploymentSequencer object
    ~DeploymentSequencer();

    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Whether a deployment is in progress
    bool isBusy() const;

    //! Start a deployment with its first attempt
    //!
    //! Does nothing if one is already in progress.
    Step start();

    //! Advance one scheduler tick
    //!
    //! Does nothing when idle. After the burn has been held for burnDurationSec ticks it stops, then either waits
    //! for the next attempt or, once maxAttempts attempts were made, finishes with FAILED. The attempts-exhausted
    //! outcome is reported as deployed (MARK_DEPLOYED): the antenna is not burned again after the last attempt.
    Step tick(const Params& params);

    //! Stop the deployment, finishing it with ABORT
    //!
    //! Does nothing when idle.
    Step stop();

  private:
    // ----------------------------------------------------------------------
    // Private types
    // ----------------------------------------------------------------------

    enum class State : std::uint8_t { IDLE, BURNING, RETRY_WAIT };

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Begin the next attempt and burn
    void startNextAttempt(Step& step);

    //! Stop the burn, and report how long it was held if it was
    void stopBurn(Step& step);

    //! End the deployment with the result
    void finish(Step& step, Result result);

    //! Advance a tick while burning
    void tickBurning(Step& step, const Params& params);

    //! Advance a tick while waiting for the next attempt
    void tickRetryWait(Step& step, const Params& params);

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    State m_state;                         //!< Where the deployment is
    std::uint32_t m_currentAttempt;        //!< Attempt in progress, 1-based, 0 when idle
    std::uint32_t m_ticksInState;          //!< Ticks since the state was entered
    std::uint32_t m_totalAttempts;         //!< Attempts started since boot, across deployments
    std::uint32_t m_burnTicksThisAttempt;  //!< Ticks the burn was held, until reported
};

}  // namespace Components
