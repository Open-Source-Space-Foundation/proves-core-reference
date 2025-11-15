#define RADIOLIB_LOW_LEVEL 1

#include "FprimeHal.hpp"

FprimeHal::FprimeHal(Components::MyComponent* component) : RadioLibHal(0, 0, 0, 0, 0, 0), m_component(component) {}

void FprimeHal::init() {}

void FprimeHal::term() {}

void FprimeHal::pinMode(uint32_t pin, uint32_t mode) {}

void FprimeHal::digitalWrite(uint32_t pin, uint32_t value) {}

uint32_t FprimeHal::digitalRead(uint32_t pin) {
    return 0;
}

void FprimeHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {}

void FprimeHal::detachInterrupt(uint32_t interruptNum) {}

void FprimeHal::delay(unsigned long ms) {}

void FprimeHal::delayMicroseconds(unsigned long us) {}

unsigned long FprimeHal::millis() {
    return 0;
}

unsigned long FprimeHal::micros() {
    return 0;
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
