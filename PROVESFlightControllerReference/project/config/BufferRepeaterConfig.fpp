# Project override of fprime-extras ExtrasConfig/BufferRepeaterConfig.fpp
module Utilities {
    @ The maximum number of fanout port pairs configured for the BufferFanout interfaces.
    constant BUFFER_FANOUT_MULTI_SIZE = 3

    @ The maximum number of buffers that can be waited on for returning
    @ issue #471: must cover the whole comms buffer pool (commsBuffCount +
    @ commsFileBuffCount), or an SD-stalled uplink overflows the in-flight map
    @ and FW_ASSERTs.
    constant BUFFER_FANOUT_MAX_BUFFERS_IN_FLIGHT = 40
}
