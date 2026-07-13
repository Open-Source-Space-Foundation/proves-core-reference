// ======================================================================
// \title  RadioLibSBandRadio.cpp
// \brief  Production SBandRadioIf implementation backed by RadioLib
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "PROVESFlightControllerReference/Components/SBand/RadioLibSBandRadio.hpp"

namespace Components {

RadioLibSBandRadio::RadioLibSBandRadio(SBand* component)
    : m_hal(component),
      m_module(&m_hal, SBAND_PIN_CS, SBAND_PIN_IRQ, SBAND_PIN_RST, SBAND_PIN_BUSY),
      m_radio(&m_module) {}

int16_t RadioLibSBandRadio::begin(float freqMHz,
                                  float bandwidthKHz,
                                  uint8_t spreadingFactor,
                                  uint8_t codingRate,
                                  uint8_t syncWord,
                                  int8_t outputPowerDbm,
                                  uint16_t preambleLength) {
    return m_radio.begin(freqMHz, bandwidthKHz, spreadingFactor, codingRate, syncWord, outputPowerDbm, preambleLength);
}

int16_t RadioLibSBandRadio::setPacketParamsLoRa(uint8_t preambleLen,
                                                uint8_t hdrType,
                                                uint8_t payLen,
                                                uint8_t crc,
                                                uint8_t invIQ) {
    return m_radio.setPacketParamsLoRa(preambleLen, hdrType, payLen, crc, invIQ);
}

int16_t RadioLibSBandRadio::transmit(const uint8_t* data, size_t len) {
    return m_radio.transmit(data, len);
}

int16_t RadioLibSBandRadio::readData(uint8_t* data, size_t len) {
    return m_radio.readData(data, len);
}

size_t RadioLibSBandRadio::getPacketLength() {
    return m_radio.getPacketLength();
}

uint16_t RadioLibSBandRadio::getIrqStatus() {
    return m_radio.getIrqStatus();
}

int16_t RadioLibSBandRadio::startReceive() {
    return m_radio.startReceive();
}

int16_t RadioLibSBandRadio::standby() {
    return m_radio.standby();
}

int16_t RadioLibSBandRadio::setSpreadingFactor(uint8_t sf) {
    return m_radio.setSpreadingFactor(sf);
}

int16_t RadioLibSBandRadio::setCodingRate(uint8_t cr) {
    return m_radio.setCodingRate(cr);
}

int16_t RadioLibSBandRadio::setBandwidth(float bwKHz) {
    return m_radio.setBandwidth(bwKHz);
}

float RadioLibSBandRadio::getRSSI() {
    return m_radio.getRSSI();
}

float RadioLibSBandRadio::getSNR() {
    return m_radio.getSNR();
}

}  // namespace Components
