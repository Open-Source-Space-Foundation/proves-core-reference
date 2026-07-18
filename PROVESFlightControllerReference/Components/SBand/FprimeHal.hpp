#ifndef FPRIME_HAL_H
#define FPRIME_HAL_H

#include <RadioLib.h>

#define FPRIME_HAL_GPIO_LEVEL_LOW 0
#define FPRIME_HAL_GPIO_LEVEL_HIGH 1

// SX1280 virtual pin numbers for RadioLib Module
// These are logical pin IDs used by the HAL to route operations to F Prime ports
#define SBAND_PIN_CS 0
#define SBAND_PIN_IRQ 5
#define SBAND_PIN_RST 6
#define SBAND_PIN_BUSY 7

namespace Components {
class SBand;
}  // namespace Components

// the HAL must inherit from the base RadioLibHal class
// and implement all of its virtual methods
class FprimeHal : public RadioLibHal {
  public:
    explicit FprimeHal(Components::SBand* component);

    void init() override;

    void term() override;

    void pinMode(uint32_t pin, uint32_t mode) override;

    void digitalWrite(uint32_t pin, uint32_t value) override;

    uint32_t digitalRead(uint32_t pin) override;

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;

    void detachInterrupt(uint32_t interruptNum) override;

    void delay(unsigned long ms) override;

    void delayMicroseconds(unsigned long us) override;

    unsigned long millis() override;

    unsigned long micros() override;

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override;

    void tone(uint32_t pin, unsigned int frequency, unsigned long duration = 0) override;

    void noTone(uint32_t pin) override;

    void spiBegin();

    void spiBeginTransaction();

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in);

    void yield() override;

    void spiEndTransaction();

    void spiEnd();

  private:
    Components::SBand* m_component;
};

#endif
