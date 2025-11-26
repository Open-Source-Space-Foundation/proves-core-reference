// ======================================================================
// \title  MyComponent.cpp
// \author jrpear
// \brief  cpp file for MyComponent component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "FprimeZephyrReference/Components/MyComponent/MyComponent.hpp"

#include <RadioLib.h>

#include <Fw/Logger/Logger.hpp>

#include "FprimeHal.hpp"

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

MyComponent ::MyComponent(const char* const compName)
    : MyComponentComponentBase(compName),
      m_rlb_hal(this),
      m_rlb_module(&m_rlb_hal, 0, 5, 0),
      m_rlb_radio(&m_rlb_module) {}

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

void MyComponent ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
    char s[] =
        "Hello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\n";
    this->txEnable_out(0, Fw::Logic::HIGH);
    state = this->m_rlb_radio.transmit(s, sizeof(s));
    // TODO replace with proper TX done interrupt handling
    Os::Task::delay(Fw::TimeInterval(0, 1000));
    this->txEnable_out(0, Fw::Logic::LOW);
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.transmit() success!\n");
    } else {
        Fw::Logger::log("radio.transmit() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::RECEIVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->rxEnable_out(0, Fw::Logic::HIGH);

    int state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    state = this->m_rlb_radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.startReceive() success!\n");
    } else {
        Fw::Logger::log("radio.startReceive() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::READ_DATA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    uint8_t buf[256] = {0};

    int16_t state = this->m_rlb_radio.readData(buf, sizeof(buf));
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.readData() success!\n");
    } else {
        Fw::Logger::log("radio.readData() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }

    Fw::Logger::log("readData() buffer:\n");

    char msg[sizeof(buf) * 3 + 1];

    for (size_t i = 0; i < sizeof(buf); ++i) {
        sprintf(msg + i * 3, "%02X ", buf[i]);  // NOLINT(runtime/printf)
    }
    msg[sizeof(buf) * 3] = '\0';

    Fw::Logger::log("%s\n", msg);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

int MyComponent ::configure_radio() {
    int state = this->m_rlb_radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setOutputPower(13);  // 13dB is max
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    // Match modulation parameters to CircuitPython defaults
    state = this->m_rlb_radio.setSpreadingFactor(7);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setBandwidth(406.25);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setCodingRate(5);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setPacketParamsLoRa(12, RADIOLIB_SX128X_LORA_HEADER_EXPLICIT, 255,
                                                  RADIOLIB_SX128X_LORA_CRC_ON, RADIOLIB_SX128X_LORA_IQ_STANDARD);
    return state;
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
