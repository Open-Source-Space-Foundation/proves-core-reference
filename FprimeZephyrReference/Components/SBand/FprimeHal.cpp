#define RADIOLIB_LOW_LEVEL 1

#include "FprimeHal.hpp"

#include <Fw/Buffer/Buffer.hpp>
#include <Fw/Time/Time.hpp>
#include <Os/Task.hpp>

#include "SBand.hpp"
#include <zephyr/kernel.h>

FprimeHal::FprimeHal(Components::SBand* component)
    : RadioLibHal(0, 0, FPRIME_HAL_GPIO_LEVEL_LOW, FPRIME_HAL_GPIO_LEVEL_HIGH, 0, 0), m_component(component) {}

void FprimeHal::init() {}

void FprimeHal::term() {}

void FprimeHal::pinMode(uint32_t pin, uint32_t mode) {}

void FprimeHal::digitalWrite(uint32_t pin, uint32_t value) {
    if (pin == SBAND_PIN_RST) {
        if (value == FPRIME_HAL_GPIO_LEVEL_LOW) {
            this->m_component->resetSend_out(0, Fw::Logic::LOW);
        } else {
            this->m_component->resetSend_out(0, Fw::Logic::HIGH);
        }
    }
}

uint32_t FprimeHal::digitalRead(uint32_t pin) {
    if (pin == SBAND_PIN_IRQ) {
        Fw::Logic irqState;
        Drv::GpioStatus state = this->m_component->getIRQLine_out(0, irqState);
        FW_ASSERT(state == Drv::GpioStatus::OP_OK);
        if (irqState == Fw::Logic::HIGH)
            return FPRIME_HAL_GPIO_LEVEL_HIGH;
        else
            return FPRIME_HAL_GPIO_LEVEL_LOW;
    }
    if (pin == SBAND_PIN_BUSY) {
        Fw::Logic busyState;
        Drv::GpioStatus state = this->m_component->getBusyLine_out(0, busyState);
        FW_ASSERT(state == Drv::GpioStatus::OP_OK);
        if (busyState == Fw::Logic::HIGH)
            return FPRIME_HAL_GPIO_LEVEL_HIGH;
        else
            return FPRIME_HAL_GPIO_LEVEL_LOW;
    }
    return FPRIME_HAL_GPIO_LEVEL_LOW;
}

void FprimeHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {}

void FprimeHal::detachInterrupt(uint32_t interruptNum) {}

void FprimeHal::delay(unsigned long ms) {
    Os::Task::delay(Fw::TimeInterval(0, ms * 1000));
}

void FprimeHal::delayMicroseconds(unsigned long us) {
    Os::Task::delay(Fw::TimeInterval(0, us));
}

unsigned long FprimeHal::millis() {
    return k_uptime_get();
}

unsigned long FprimeHal::micros() {
    return k_uptime_get() * 1000;
}

long FprimeHal::pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) {
    return 0;
}

void FprimeHal::tone(uint32_t pin, unsigned int frequency, unsigned long duration) {}

void FprimeHal::noTone(uint32_t pin) {}

void FprimeHal::spiBegin() {}

void FprimeHal::spiBeginTransaction() {}

void FprimeHal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
    Fw::Buffer writeBuffer(out, len);
    Fw::Buffer readBuffer(in, len);
    this->m_component->spiSend_out(0, writeBuffer, readBuffer);
}

void FprimeHal::yield() {}

void FprimeHal::spiEndTransaction() {}

void FprimeHal::spiEnd() {}
