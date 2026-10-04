#ifndef LORA_CFG_HPP
#define LORA_CFG_HPP
#include <Fw/FPrimeBasicTypes.hpp>

#include <zephyr/drivers/lora.h>
namespace LoRaConfig {
const U32 FREQUENCY = 437400000;               //!< LoRa frequency in Hz
const U32 MIN_FREQUENCY = 430000000;           //!< Minimum LoRa frequency in Hz
const U32 MAX_FREQUENCY = 440000000;           //!< Maximum LoRa frequency in Hz
lora_signal_bandwidth BANDWIDTH = BW_125_KHZ;  //!< LoRa bandwidth
const I8 TX_POWER = 23;                        //!< LoRa transmission power in dBm
const U16 PREAMBLE_LENGTH = 8;                 //!< LoRa preamble length
U8 HEADER[] = {0, 0, 0, 0};                    //!< LoRa header (not used)
}  // namespace LoRaConfig
#endif  // LORA_CFG_HPP
