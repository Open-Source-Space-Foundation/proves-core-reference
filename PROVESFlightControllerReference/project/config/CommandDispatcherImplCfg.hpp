/*
 * CmdDispatcherImplCfg.hpp
 *
 *  Created on: May 6, 2015
 *      Author: tcanham
 */

#ifndef CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_
#define CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_

// Define configuration values for dispatcher

enum {
    // !< The size of the table holding opcodes to dispatch. Must be >= the
    // number of commands in the deployment (351 as of MosaicManager); when the
    // table fills, the next compCmdReg_handler registration FW_ASSERTs and the
    // FSW FATALs during topology setup rather than failing at build time.
    CMD_DISPATCHER_DISPATCH_TABLE_SIZE = 400,
    CMD_DISPATCHER_SEQUENCER_TABLE_SIZE = 10,  // !< The size of the table holding commands in progress
};

#endif /* CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_ */
