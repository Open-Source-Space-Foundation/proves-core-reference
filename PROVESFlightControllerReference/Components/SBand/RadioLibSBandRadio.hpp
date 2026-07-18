// ======================================================================
// \title  RadioLibSBandRadio.hpp
// \brief  Production SBandRadioIf implementation backed by RadioLib
// ======================================================================

#ifndef Components_RadioLibSBandRadio_HPP
#define Components_RadioLibSBandRadio_HPP

#include <RadioLib.h>

#include "PROVESFlightControllerReference/Components/SBand/FprimeHal.hpp"
#include "PROVESFlightControllerReference/Components/SBand/SBandRadioIf.hpp"

namespace Components {

class SBand;

//! Production implementation of SBandRadioIf: owns the RadioLib HAL/Module/
//! SX1280 stack and forwards every call 1:1 (S-BAND-REINTEGRATION-PLAN.md
//! decision D4). The SBand component holds this by value and points its
//! SBandRadioIf* at it in production; tests point the same pointer at a
//! fake instead.
class RadioLibSBandRadio final : public SBandRadioIf {
  public:
    //! \param component back-pointer used by the owned FprimeHal to reach
    //!        the component's GPIO/SPI output ports.
    explicit RadioLibSBandRadio(SBand* component);

    int16_t begin(float freqMHz,
                  float bandwidthKHz,
                  uint8_t spreadingFactor,
                  uint8_t codingRate,
                  uint8_t syncWord,
                  int8_t outputPowerDbm,
                  uint16_t preambleLength) override;

    int16_t setPacketParamsLoRa(uint8_t preambleLen,
                                uint8_t hdrType,
                                uint8_t payLen,
                                uint8_t crc,
                                uint8_t invIQ) override;

    int16_t transmit(const uint8_t* data, size_t len) override;
    int16_t readData(uint8_t* data, size_t len) override;
    size_t getPacketLength() override;
    uint16_t getIrqStatus() override;
    int16_t startReceive() override;
    int16_t standby() override;
    int16_t setSpreadingFactor(uint8_t sf) override;
    int16_t setCodingRate(uint8_t cr) override;
    int16_t setBandwidth(float bwKHz) override;
    float getRSSI() override;
    float getSNR() override;

  private:
    FprimeHal m_hal;  //!< RadioLib HAL instance
    Module m_module;  //!< RadioLib Module instance
    SX1280 m_radio;   //!< RadioLib SX1280 radio instance
};

}  // namespace Components

#endif
