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

// Temporary includes for Zephyr GPIO access before integrating with F' GPIO driver
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

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

    uint8_t buf[256] = {0};

    // cannot specify timeout greater than 2^16 * 15.625us = 1024 ms as timeout
    // is internally resolved to 16-bit representation of 15.625us step count
    state = this->m_rlb_radio.receive(buf, sizeof(buf), RadioLibTime_t(1024 * 1000));
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.receive() success!\n");
    } else {
        Fw::Logger::log("radio.receive() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }

    Fw::Logger::log("RESULTING BUFFER:\n");

    char msg[sizeof(buf) * 3 + 1];

    for (size_t i = 0; i < sizeof(buf); ++i) {
        sprintf(msg + i * 3, "%02X ", buf[i]);  // NOLINT(runtime/printf)
    }
    msg[sizeof(buf) * 3] = '\0';

    Fw::Logger::log("%s\n", msg);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

static void rf2_io1_isr(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    Fw::Logger::log("RF2 IO1 Interrupt Triggered!\n");
}

static int rf2_io1_init(void) {
#define RF2_IO1_NODE DT_NODELABEL(rf2_io1)
    static const struct gpio_dt_spec rf2_io1 = GPIO_DT_SPEC_GET(RF2_IO1_NODE, gpios);

    static struct gpio_callback rf2_io1_cb;

    int ret;

    Fw::Logger::log("Initializing RF2 IO1 GPIO...\n");
    if (!device_is_ready(rf2_io1.port)) {
        return -ENODEV;
    }

    Fw::Logger::log("RF2 IO1 GPIO device is ready.\n");
    /* Configure as input (flags from DT are already in rf2_io1.dt_flags) */
    ret = gpio_pin_configure_dt(&rf2_io1, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    Fw::Logger::log("RF2 IO1 GPIO configured as input.\n");
    /* Interrupt when the line goes active (high in your case) */
    // ret = gpio_pin_interrupt_configure_dt(&rf2_io1,
    //                                       GPIO_INT_EDGE_TO_ACTIVE);
    ret = gpio_pin_interrupt_configure_dt(&rf2_io1, GPIO_INT_EDGE_BOTH);
    Fw::Logger::log("ret after interrupt configure: %d\n", ret);
    // I get -134, ENOTSUP and I believe the reason is that mcp23017 inta and
    // intb pins are not connected the MCU
    if (ret < 0) {
        return ret;
    }

    Fw::Logger::log("RF2 IO1 GPIO interrupt configured.\n");
    /* Register callback */
    gpio_init_callback(&rf2_io1_cb, rf2_io1_isr, BIT(rf2_io1.pin));
    gpio_add_callback(rf2_io1.port, &rf2_io1_cb);

    return 0;
}

void MyComponent ::READ_DATA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    static int init_done = rf2_io1_init();
    FW_ASSERT(init_done == 0);

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

}  // namespace Components
