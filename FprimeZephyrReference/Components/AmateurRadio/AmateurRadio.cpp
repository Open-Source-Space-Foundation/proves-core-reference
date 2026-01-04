// ======================================================================
// \title  AmateurRadio.cpp
// \author t38talon
// \brief  cpp file for AmateurRadio component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/AmateurRadio/AmateurRadio.hpp"

#include <string>

#include "FprimeZephyrReference/Components/AmateurRadio/JokesList.hpp"
#include <zephyr/random/random.h>
namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AmateurRadio ::AmateurRadio(const char* const compName) : AmateurRadioComponentBase(compName), m_count_names(0) {}

AmateurRadio ::~AmateurRadio() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void AmateurRadio ::Repeat_Name_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& radio_name) {
    // Increment the count
    this->m_count_names++;

    // Get current time for telemetry
    Fw::Time time = this->getTime();

    // Write telemetry
    this->tlmWrite_count_names(this->m_count_names, time);

    // Emit event
    Fw::LogStringArg radioNameArg(radio_name.toChar());
    Fw::LogStringArg satNameArg("Sat1");
    this->log_ACTIVITY_HI_ReapeatingName(radioNameArg, satNameArg);

    // Send command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AmateurRadio ::TELL_JOKE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Get a random joke
    const FwSizeType jokeIndex = sys_rand32_get() % NUM_JOKES;
    const char* joke = JOKES[jokeIndex];

    // Log the joke as an event
    Fw::LogStringArg jokeArg(joke);
    this->log_ACTIVITY_HI_JokeTold(jokeArg);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
