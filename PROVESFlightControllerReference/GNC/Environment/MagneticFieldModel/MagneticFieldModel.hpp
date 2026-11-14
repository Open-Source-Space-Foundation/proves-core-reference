/**
 * \file MagneticFieldModel.hpp
 * \brief WMM2025 evaluated at the propagated orbit position.
 */

#ifndef Gnc_Environment_MagneticFieldModel_HPP
#define Gnc_Environment_MagneticFieldModel_HPP

#include "PROVESFlightControllerReference/GNC/Environment/MagneticFieldModel/MagneticFieldModelComponentAc.hpp"
#include "WmmModel.hpp"

namespace Gnc {
namespace Environment {

/**
 * \brief Framework wrapper around the WMM field model.
 *
 * \details Produces a reference magnetic field vector.
 *  Driven by the arrival of OrbitState.
 *  Framework wrapper, all field calculations live in WmmModel.cpp.
 */
class MagneticFieldModel final : public MagneticFieldModelComponentBase {
  public:
    /**
     * \brief Construct the component with a zeroed, invalid field.
     * \param compName  F Prime component instance name
     */
    explicit MagneticFieldModel(const char* const compName);

    //! Destroy the component. Holds no resources.
    ~MagneticFieldModel();

  private:
    /*
     * ============================================================================
     * Handlers for typed input ports
     * ============================================================================
     */

    /**
     * \brief An orbit state arrived. The trigger for a new field
     *        evaluation.
     *
     * \details Gated on the OrbitState's positionUsable flag, then
     * handed to evaluateField(). Last valid evaluation publish on an invalid OrbitState.
     *
     * \param portNum  Port index, unused (single port)
     * \param state    Orbit state from OrbitPropagator
     */
    void orbitIn_handler(FwIndexType portNum, Gnc::OrbitState& state) override;

    /**
     * \brief Telemetry heartbeat.
     *
     * \details
     *
     * \param portNum  Port index, unused (single port)
     * \param context  Rate group context, unused
     */
    void run_handler(FwIndexType portNum, U32 context) override;

    /*
     * ============================================================================
     * Internals
     * ============================================================================
     */

    /**
     * \brief Push the field evaluation, or the last good result, tagged as stale.
     *
     * \details The full field vector is pushed to all consumers.
     * If a consumer needs a unit vector, it needs to normalize it.
     *
     * \param sol    Field to publish. On the invalid path this is the
     *               last good solution.
     * \param valid  Whether this cycle produced a fresh evaluation
     * \param stamp  Epoch of the orbit state.
     */
    void emit(const FieldSolution& sol, bool valid, const Fw::Time& stamp);

    /*
     * ============================================================================
     * State
     *
     * Touched by two threads: the propagator thread via orbitIn and
     * the rate group thread via run. The component's input ports are guarded because of this.
     * ============================================================================
     */

    FieldSolution m_lastField;  //!< Most recent successful evaluation
    F32 m_lastDecYear = 0.0f;   //!< Decimal year of the most recent evaluation, successful or not
    bool m_lastValid = false;   //!< Whether the most recent evaluation succeeded
    bool m_everValid = false;   //!< Whether any evaluation has ever succeeded
    U32 m_rejectCount = 0;      //!< Cumulative rejected evaluations, any reason
};

}  // namespace Environment
}  // namespace Gnc

#endif
