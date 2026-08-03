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
static const FwChanIdType MAX_PACKETIZER_PACKETS = 22;

<<<<<<< HEAD
static const FwChanIdType TLMPACKETIZER_HASH_BUCKETS =
    220;  // !< Buckets assignable to a hash slot.
          // Buckets must be >= number of telemetry channels in system (211 as of MosaicManager/data products)
=======
static const FwChanIdType MAX_PACKETIZER_CHANNELS =
    202;  // !< Must be >= number of non-omitted telemetry channels in system

>>>>>>> 451c5bcb74e38cc23942521ad2d1223b34a5aca9
static const FwChanIdType TLMPACKETIZER_MAX_MISSING_TLM_CHECK =
    25;  // !< Maximum number of missing telemetry channel checks
}  // namespace Svc

#endif /* SVC_TLMPACKETIZER_TLMPACKETIZERCOMPONENTIMPLCFG_HPP_ */
