#define RADIOLIB_LOW_LEVEL 1

#include "FprimeHal.hpp"

#include <Fw/Logger/Logger.hpp>

FprimeHal::FprimeHal(Components::MyComponent* component)
    : RadioLibHal(0, 0, GPIO_LEVEL_LOW, GPIO_LEVEL_HIGH, 0, 0), m_component(component) {}

void FprimeHal::init() {}

void FprimeHal::term() {}

void FprimeHal::pinMode(uint32_t pin, uint32_t mode) {}

void FprimeHal::digitalWrite(uint32_t pin, uint32_t value) {
    Fw::Logger::log("digitalWrite pin %u value %u\n", pin, value);
    if (pin == RST_PIN) {
        Fw::Logic state = (value == GPIO_LEVEL_HIGH) ? Fw::Logic::HIGH : Fw::Logic::LOW;
        this->m_component->resetSend_out(0, state);
    }
}

uint32_t FprimeHal::digitalRead(uint32_t pin) {
    Fw::Logger::log("digitalRead pin %u\n", pin);
    if (pin == BUSY_PIN) {
        Fw::Logic state;
        Drv::GpioStatus status = this->m_component->gpioBusyRead_out(0, state);
        FW_ASSERT(status == Drv::GpioStatus::OP_OK);
        return (state == Fw::Logic::HIGH) ? 1 : 0;
    }
    return 0;
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
    Fw::Time time = this->m_component->getTime();
    return time.getSeconds() * 1000 + time.getUSeconds() / 1000;
}

unsigned long FprimeHal::micros() {
    Fw::Time time = this->m_component->getTime();
    return time.getSeconds() * 1000000 + time.getUSeconds();
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
