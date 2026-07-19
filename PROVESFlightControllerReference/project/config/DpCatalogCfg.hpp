/*
 * DpCatalogCfg.hpp:
 *
 * Configuration settings for the DpCatalog component.
 */

#ifndef SVC_DPCATALOG_CONFIG_HPP_
#define SVC_DPCATALOG_CONFIG_HPP_
#include <Fw/FPrimeBasicTypes.hpp>

namespace Svc {
// Sets the maximum number of directories where
// data products can be stored. The array passed
// to the initializer for DpCatalog cannot exceed
// this size.
static const FwIndexType DP_MAX_DIRECTORIES = 2;
// The catalog allocates DP_MAX_FILES slots (~140 B each) in one contiguous
// block from the libc heap at configure time; the fprime default of 127
// (~17 KB) fails on the RP2350 after the topology's queues and com stacks
// have claimed most of the arena.
static const FwIndexType DP_MAX_FILES = 32;
}  // namespace Svc

#endif /* SVC_DPCATALOG_CONFIG_HPP_ */
