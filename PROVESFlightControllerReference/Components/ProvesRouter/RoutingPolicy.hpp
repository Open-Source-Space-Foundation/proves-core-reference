// ======================================================================
// \title  RoutingPolicy.hpp
// \brief  Pure decisions ProvesRouter makes about a packet it has accepted
// ======================================================================

#pragma once

namespace Components {
namespace RoutingPolicy {

//! Whether a packet holds off the command-loss dead-man
//!
//! Only authenticated traffic counts. A packet let through by the
//! authentication bypass (NO_OP, GET_SEQ_NUM, ...) can be sent by anyone,
//! so it must not prove that the ground still controls the spacecraft.
constexpr bool resetsCommandLoss(bool authenticated, bool routed) {
    return authenticated && routed;
}

}  // namespace RoutingPolicy
}  // namespace Components
