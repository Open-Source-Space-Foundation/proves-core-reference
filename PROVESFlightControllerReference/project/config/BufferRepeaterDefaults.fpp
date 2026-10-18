# Project override of fprime-extras FprimeExtrasConfig/BufferRepeaterDefaults/BufferRepeaterDefaults.fpp
module Utilities {
    @ Default enable state for a BufferRepeater output channel
    constant BUFFER_REPEATER_DEFAULT_CHANNEL_ENABLE = Fw.Enabled.DISABLED

    @ Per-channel default of downlinkRepeater.CHANNEL_ENABLED (BUFFER_FANOUT_MULTI_SIZE entries).
    @ File downlink goes to the LoRa link only: the UART line (ground over USB) and the S-band
    @ line are selected by command/sequence (downlinkRepeater.CHANNEL_ENABLED_PRM_SET) when needed.
    constant BUFFER_REPEATER_DEFAULT_CHANNEL_ENABLES = [
        Fw.Enabled.DISABLED, # Channel 0: ComCcsdsUart.comQueue
        Fw.Enabled.ENABLED,  # Channel 1: ComCcsdsLora.comQueue
        Fw.Enabled.DISABLED, # Channel 2: ComCcsdsSband.comQueue
    ]
}
