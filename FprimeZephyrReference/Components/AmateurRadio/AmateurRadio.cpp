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
