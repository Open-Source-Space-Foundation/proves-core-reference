/**
 * \file MagneticFieldModel.cpp
 * \brief WMM2025 evaluated at the propagated orbit position.
 *
 * \details FPrime layer for the WMM evaluation:
 * - Handles events, telemetry and fault reporting.
 * - Holds the last valid WMM field
 * - Validates incoming OrbitStates before calling the WmmModel
 *
 * Per-arrival pipeline:
 *   1. Gate on OrbitState::positionUsable
 *   2. Pass the position + epoch + GMST to WMM, receive the field vector in nT(TEME): evaluateField()
 *   3. Publish the reference magnetic field vector: emit()
 */

#include "MagneticFieldModel.hpp"

#include "PROVESFlightControllerReference/GNC/Types/GncConvert.hpp"

namespace Gnc {
namespace Environment {

/*
 * ============================================================================
 * Construction
 * ============================================================================
 */

/**
 * \brief Construct the component with a zeroed, invalid field.
 * \param compName  F Prime component instance name
 */
MagneticFieldModel::MagneticFieldModel(const char* const compName) : MagneticFieldModelComponentBase(compName) {}

//! Destroy the component. Holds no resources.
MagneticFieldModel::~MagneticFieldModel() {}

/*
 * ============================================================================
 * OrbitState Arrival and Validation + Field Evaluation
 * ============================================================================
 */

/**
 * \brief Orbit state arrived. Evaluate the field once.
 *
 * \details publishes the last valid field evaluation when the incoming orbit is invalid.
 *
 * \param portNum  Port index, unused
 * \param state    Orbit state from OrbitPropagator
 */
void MagneticFieldModel::orbitIn_handler(FwIndexType portNum, Gnc::OrbitState& state) {
    (void)portNum;

    // Load WMM coefficients once, after the component starts running.
    if (!m_wmmLoadAttempted) {
        m_wmmLoadAttempted = true;

        m_wmmLoadedFromFile =
            loadWmmCoefficients("WMM.COF", m_wmmModel);
    }

    const Fw::Time stamp = state.get_stamp();

    // Check if the position is valid
    if (!state.get_positionUsable()) {
        this->log_WARNING_LO_OrbitUnusable(state.get_validity());
        m_lastValid = false;
        m_rejectCount++;
        this->emit(m_lastField, false, stamp);
        return;
    }

    FieldQuery query;
    query.posTemeKm = toAstro(state.get_posTemeKm());
    query.altKm = state.get_altKm();
    query.jdUt1 = state.get_jdUt1();
    query.gmstRad = state.get_gmstRad();

    FieldSolution sol;

    FieldConfig config;
    config.minYear = m_wmmModel.epoch;
    config.maxYear = m_wmmModel.epoch + 5.0f;

    const FieldResult result =
        evaluateField(query, config, sol, m_wmmModel);

    m_lastDecYear = sol.decYear;

    if (result != FieldResult::OK) {
        // Corrupt field evaluation
        if (result == FieldResult::DEGENERATE_FIELD) {
            this->log_WARNING_HI_FieldDegenerate(static_cast<F32>(sol.magnitudeNt));
        }
        // Out of range altitude or decimal year
        else {
            this->log_WARNING_LO_EvaluationOutOfRange(static_cast<F32>(state.get_altKm()), sol.decYear);
        }

        m_lastValid = false;
        m_rejectCount++;
        this->emit(m_lastField, false, stamp);
        return;
    }

    if (!m_lastValid && m_everValid) {
        this->log_ACTIVITY_HI_FieldRestored();
    }

    m_lastField = sol;
    m_lastValid = true;
    m_everValid = true;

    this->emit(sol, true, stamp);
}

/*
 * ============================================================================
 * Publication
 * ============================================================================
 */

/**
 * \brief Publish the field vector on fieldOut.
 *
 * \param sol    Field to publish, fresh or cached
 * \param valid  Whether this cycle produced a fresh evaluation
 * \param stamp  Epoch of the orbit state
 */
void MagneticFieldModel::emit(const FieldSolution& sol, bool valid, const Fw::Time& stamp) {
    // Pack the sample for all consumers
    Gnc::VectorSample sample;
    sample.set_vec(toVec3f(sol.fieldTemeNt));
    sample.set_frame(Gnc::FrameId::TEME);
    sample.set_stamp(stamp);
    sample.set_valid(valid);

    // Push the sample to all connected ports
    const FwIndexType ports = getNum_fieldOut_OutputPorts();

    for (FwIndexType p = 0; p < ports; ++p) {
        if (this->isConnected_fieldOut_OutputPort(p)) {
            Gnc::VectorSample copy = sample;
            this->fieldOut_out(p, copy);
        }
    }
}

/**
 * \brief Telemetry heartbeat. Reports cached state.
 *
 * \param portNum  Port index, unused
 * \param context  Rate group context, unused
 */
void MagneticFieldModel::run_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    // Only reports after the first field computation.
    if (m_everValid) {
        this->tlmWrite_FieldTemeNt(toVec3f(m_lastField.fieldTemeNt));
        this->tlmWrite_FieldMagnitudeNt(static_cast<F32>(m_lastField.magnitudeNt));
        this->tlmWrite_EvalDecYear(m_lastDecYear);
    }

    this->tlmWrite_FieldValid(m_lastValid);
    this->tlmWrite_FieldRejectCount(m_rejectCount);
}

}  // namespace Environment
}  // namespace Gnc
