// ======================================================================
// \title  SBandRadioIf.hpp
// \brief  Abstract seam over the SX1280 (RadioLib) surface used by SBand
// ======================================================================

#ifndef Components_SBandRadioIf_HPP
#define Components_SBandRadioIf_HPP

#include <cstddef>
#include <cstdint>

namespace Components {

//! Abstract interface covering exactly the SX1280 (RadioLib) surface the
//! SBand component uses (S-BAND-REINTEGRATION-PLAN.md decision D4). No
//! Zephyr or F Prime autocode dependencies -- just stdint/cstddef -- so it
//! compiles anywhere: production code (RadioLibSBandRadio, which owns
//! FprimeHal/Module/SX1280 and implements this 1:1) and test fakes alike.
//!
//! The component holds a pointer to this interface rather than the RadioLib
//! types directly, so its handlers can be exercised against a scripted fake
//! without any RadioLib/Zephyr headers in the mix.
class SBandRadioIf {
  public:
    virtual ~SBandRadioIf() = default;

    //! Configure and start the radio. Mirrors SX128x::begin.
    virtual int16_t begin(float freqMHz,
                          float bandwidthKHz,
                          uint8_t spreadingFactor,
                          uint8_t codingRate,
                          uint8_t syncWord,
                          int8_t outputPowerDbm,
                          uint16_t preambleLength) = 0;

    //! Mirrors SX128x::setPacketParamsLoRa.
    virtual int16_t setPacketParamsLoRa(uint8_t preambleLen,
                                        uint8_t hdrType,
                                        uint8_t payLen,
                                        uint8_t crc,
                                        uint8_t invIQ) = 0;

    //! Mirrors SX128x::transmit.
    virtual int16_t transmit(const uint8_t* data, size_t len) = 0;

    //! Mirrors SX128x::readData.
    virtual int16_t readData(uint8_t* data, size_t len) = 0;

    //! Mirrors SX128x::getPacketLength.
    virtual size_t getPacketLength() = 0;

    //! Mirrors SX128x::getIrqStatus.
    virtual uint16_t getIrqStatus() = 0;

    //! Mirrors SX128x::startReceive (no-arg overload: continuous Rx mode).
    virtual int16_t startReceive() = 0;

    //! Mirrors SX128x::standby.
    virtual int16_t standby() = 0;

    //! Mirrors SX128x::setSpreadingFactor.
    virtual int16_t setSpreadingFactor(uint8_t sf) = 0;

    //! Mirrors SX128x::setCodingRate.
    virtual int16_t setCodingRate(uint8_t cr) = 0;

    //! Mirrors SX128x::setBandwidth.
    virtual int16_t setBandwidth(float bwKHz) = 0;

    //! Mirrors SX128x::getRSSI.
    virtual float getRSSI() = 0;

    //! Mirrors SX128x::getSNR.
    virtual float getSNR() = 0;
};

}  // namespace Components

#endif
