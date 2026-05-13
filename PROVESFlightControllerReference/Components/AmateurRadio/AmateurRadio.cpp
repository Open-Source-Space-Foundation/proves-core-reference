// ======================================================================
// \title  AmateurRadio.cpp
// \author t38talon
// \brief  cpp file for AmateurRadio component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/AmateurRadio/AmateurRadio.hpp"

#include <zephyr/random/random.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AmateurRadio::AmateurRadio(const char* const compName) : AmateurRadioComponentBase(compName), m_count_names(0) {}

AmateurRadio::~AmateurRadio() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void AmateurRadio::TELL_JOKE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const U32 jokeIndex = sys_rand32_get() % NUM_JOKES;

    switch (jokeIndex) {
        case 0:
            this->log_ACTIVITY_HI_Joke0();
            break;
        case 1:
            this->log_ACTIVITY_HI_Joke1();
            break;
        case 2:
            this->log_ACTIVITY_HI_Joke2();
            break;
        case 3:
            this->log_ACTIVITY_HI_Joke3();
            break;
        case 4:
            this->log_ACTIVITY_HI_Joke4();
            break;
        case 5:
            this->log_ACTIVITY_HI_Joke5();
            break;
        case 6:
            this->log_ACTIVITY_HI_Joke6();
            break;
        case 7:
            this->log_ACTIVITY_HI_Joke7();
            break;
        case 8:
            this->log_ACTIVITY_HI_Joke8();
            break;
        case 9:
            this->log_ACTIVITY_HI_Joke9();
            break;
        case 10:
            this->log_ACTIVITY_HI_Joke10();
            break;
        case 11:
            this->log_ACTIVITY_HI_Joke11();
            break;
        case 12:
            this->log_ACTIVITY_HI_Joke12();
            break;
        case 13:
            this->log_ACTIVITY_HI_Joke13();
            break;
        case 14:
            this->log_ACTIVITY_HI_Joke14();
            break;
        case 15:
            this->log_ACTIVITY_HI_Joke15();
            break;
        default:
            this->log_ACTIVITY_HI_Joke0();
            break;
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
