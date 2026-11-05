/*
 * TlmPacketizerComponentImplCfg.hpp
 *
 *  Created on: Dec 10, 2017
 *      Author: tim
 */

// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.

#ifndef SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_
#define SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Svc {
static const FwChanIdType MAX_PACKETIZER_PACKETS = 23;

// Every channel in the dictionary takes a slot, omitted ones included, and setPacketList asserts at boot
// when they do not fit. 221 channels as of ReadinessMonitor; the rest is headroom.
static const FwChanIdType MAX_PACKETIZER_CHANNELS = 232;

static const FwChanIdType TLMPACKETIZER_MAX_MISSING_TLM_CHECK =
    25;  // !< Maximum number of missing telemetry channel checks
}  // namespace Svc

#endif /* SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_ */
