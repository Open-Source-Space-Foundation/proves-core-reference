// ======================================================================
// \title  MyComponent.cpp
// \author jrpear
// \brief  cpp file for MyComponent component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "FprimeZephyrReference/Components/MyComponent/MyComponent.hpp"

#include <RadioLib.h>

#include <Fw/Logger/Logger.hpp>

#define OP_SET_MODULATION_PARAMS 0x8B
#define OP_SET_TX_PARAMS 0x8E
#define OP_SET_CONTINUOUS_PREAMBLE 0xD2
#define OP_SET_CONTINUOUS_WAVE 0xD1
#define OP_SET_RF_FREQUENCY 0x86
#define OP_SET_TX 0x83
#define OP_SET_STANDBY 0x80
#define OP_GET_STATUS 0xC0

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MyComponent ::MyComponent(const char* const compName) : MyComponentComponentBase(compName) {}

MyComponent ::~MyComponent() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void MyComponent ::run_handler(FwIndexType portNum, U32 context) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void MyComponent ::FOO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    FprimeHal hal(this);
    Module m(&hal, 0, 0, 0);
    SX1280 radio(&m);
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.begin() success!\n");
    } else {
        Fw::Logger::log("radio.begin() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // BROKEN
    this->resetSend_out(0, Fw::Logic::HIGH);
    Os::Task::delay(Fw::TimeInterval(0, 1000));
    this->resetSend_out(0, Fw::Logic::LOW);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// SPI Commands

void MyComponent ::spiSetModulationParams() {
    //                                           Bitrate+Bandwidth, Modulation Index, Shaping
    U8 write_data[] = {OP_SET_MODULATION_PARAMS, 0x4C, 0x0F, 0x00};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetTxParams() {
    //                                   power, rampTime
    U8 write_data[] = {OP_SET_TX_PARAMS, 0x1F, 0x80};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetTxContinuousPreamble() {
    U8 write_data[] = {OP_SET_CONTINUOUS_PREAMBLE};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetTxContinuousWave() {
    U8 write_data[] = {OP_SET_CONTINUOUS_WAVE};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetRfFrequency() {
    constexpr long double target_freq = 2405000000;                                   // 2.405 GHz
    constexpr long double step = 52000000.0L / static_cast<long double>(1ULL << 18);  // per datasheet
    constexpr U32 steps = static_cast<U32>(target_freq / step);
    static_assert(steps <= 0xFFFFFF, "Result must fit in 24 bits (<= 0xFFFFFF).");
    uint8_t b0 = (uint8_t)(steps & 0xFFu);  // least significant byte
    uint8_t b1 = (uint8_t)((steps >> 8) & 0xFFu);
    uint8_t b2 = (uint8_t)((steps >> 16) & 0xFFu);

    U8 write_data[] = {OP_SET_RF_FREQUENCY, b2, b1, b0};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetTx() {
    //                            periodBase (1ms), periodBaseCount[15:8], periodBaseCount[7:0]
    U8 write_data[] = {OP_SET_TX, 0x02, 0x4, 0x00};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

void MyComponent ::spiSetStandby() {
    //                                 config
    U8 write_data[] = {OP_SET_STANDBY, 0x00};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
}

U8 MyComponent ::spiGetStatus() {
    U8 write_data[] = {OP_GET_STATUS, 0x00};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(0, writeBuffer, readBuffer);
    return read_data[sizeof(read_data) - 1];
}

}  // namespace Components
