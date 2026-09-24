/*
 * CmdDispatcherImplCfg.hpp
 *
 *  Created on: May 6, 2015
 *      Author: tcanham
 */

#ifndef CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_
#define CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

// Define configuration values for dispatcher

enum {
    // Must be >= the deployment's total command count (see the dictionary's
    // `commands` array).  CommandDispatcher inserts every opcode into a
    // fixed-capacity RedBlackTreeMap and FW_ASSERTs when the insert fails
    // (CommandDispatcherImpl.cpp:35), so exceeding this bricks the boot rather
    // than degrading.  Adding PROVISION_KEY/ADD_KEY/REMOVE_KEY to the
    // TcSecurityDeframer instances pushed the count from 348 to 354; headroom
    // raised here so the next few commands do not have to repeat this.
    CMD_DISPATCHER_DISPATCH_TABLE_SIZE = 512,  // !< The size of the table holding opcodes to dispatch
    CMD_DISPATCHER_SEQUENCER_TABLE_SIZE = 10,  // !< The size of the table holding commands in progress
};

namespace Svc {
namespace CmdDispatcherCfg {

//! Include command opcodes in events when true.
//! When false, opcode fields are set to the maximum FwOpcodeType value.
constexpr bool IncludeCommandOpcodesInEvents = true;

constexpr FwOpcodeType getEventOpcode(const FwOpcodeType opcode) {
    return IncludeCommandOpcodesInEvents ? opcode : std::numeric_limits<FwOpcodeType>::max();
}

}  // namespace CmdDispatcherCfg
}  // namespace Svc

#endif /* CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_ */
