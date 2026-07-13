// ======================================================================
// \title  SBandFaultPolicy.cpp
// \brief  cpp file for SBandFaultPolicy pure-logic class
// ======================================================================

#include "PROVESFlightControllerReference/Components/SBand/SBandFaultPolicy.hpp"

namespace Components {

SBandFaultPolicy::SBandFaultPolicy()
    : m_consecutiveFailures(0), m_resetsSinceSuccess(0), m_resetPending(false), m_faulted(false) {}

void SBandFaultPolicy::operationSucceeded() {
    if (m_faulted) {
        return;
    }
    m_consecutiveFailures = 0;
    m_resetsSinceSuccess = 0;
}

void SBandFaultPolicy::operationFailed() {
    if (m_faulted) {
        return;
    }
    m_consecutiveFailures++;
    if ((m_consecutiveFailures >= CONSECUTIVE_FAILURE_LIMIT) && !m_resetPending) {
        m_resetPending = true;
    }
}

void SBandFaultPolicy::resetCompleted() {
    if (m_faulted || !m_resetPending) {
        return;
    }
    m_resetPending = false;
    m_consecutiveFailures = 0;
    m_resetsSinceSuccess++;
    if (m_resetsSinceSuccess >= RESET_ATTEMPT_LIMIT) {
        m_faulted = true;
    }
}

void SBandFaultPolicy::groundResetRequested() {
    m_faulted = false;
    m_consecutiveFailures = 0;
    m_resetsSinceSuccess = 0;
    m_resetPending = false;
}

SBandFaultPolicy::Decision SBandFaultPolicy::decision() const {
    if (m_faulted) {
        return Decision::FAULTED;
    }
    if (m_resetPending) {
        return Decision::REQUEST_RESET;
    }
    return Decision::NONE;
}

}  // namespace Components
