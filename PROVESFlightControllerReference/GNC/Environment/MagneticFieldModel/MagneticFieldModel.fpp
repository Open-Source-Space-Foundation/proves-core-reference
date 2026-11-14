# ======================================================================
# MagneticFieldModel.fpp
#
# WMM2025 evaluated at the propagated orbit position.
#
# Passive. Driven by OrbitPropagator.
# Rate group used only for telemetry.
# ======================================================================

module Gnc {
module Environment {

  passive component MagneticFieldModel {

    # ------------------------------------------------------------------
    # Data ports
    # ------------------------------------------------------------------

    @ Orbit state from OrbitPropagator. Arrival triggers an evaluation.
    guarded input port orbitIn: Gnc.OrbitStateSend

    @ Magnetic field vector with magnitude, TEME, nT.
    output port fieldOut: Gnc.VectorSampleSend


    @ Rate group tick. Telemetry only.
    guarded input port run: Svc.Sched

    # ------------------------------------------------------------------
    # Telemetry
    # ------------------------------------------------------------------

    @ Last computed field vector, TEME, nanotesla
    telemetry FieldTemeNt: Gnc.Vec3f id 0x00

    @ Field magnitude, nanotesla.
    telemetry FieldMagnitudeNt: F32  id 0x01 format "{.1f}"

    @ Decimal year the model was last evaluated at
    telemetry EvalDecYear: F32 id 0x02 format "{.4f}"

    @ True if the last evaluation produced a usable field
    telemetry FieldValid: bool id 0x03

    @ Count of evaluations rejected as out of range.
    telemetry FieldRejectCount: U32 id 0x04

    # ------------------------------------------------------------------
    # Events
    # ------------------------------------------------------------------

    @ Emitted if a field request is outside the model's validated
    @ altitude or epoch range. Throttled to limit identical events from
    @ an out of range orbit.
    event EvaluationOutOfRange(
                             altitudeKm: F32
                             decYear: F32
                           ) \
      severity warning low \
      id 0x00 \
      format "WMM query outside validated range: alt={f} km, year={f}" \
      throttle 5

    @ The orbit state carries no usable position, so the field can't be
    @ computed.
    event OrbitUnusable(
                         validity: Gnc.OrbitValidity
                       ) \
      severity warning low \
      id 0x01 \
      format "No field: orbit validity {}" \
      throttle 5

    @ Field evaluation recovered after a dropout
    event FieldRestored \
      severity activity high \
      id 0x02 \
      format "Magnetic field evaluation restored"

    @ Field evaluation returned a magnitude too small to be physical.
    @ Indicates a corrupt evaluation.
    event FieldDegenerate(
                          magnitudeNt: F32
                        ) \
      severity warning high \
      id 0x03 \
      format "WMM returned |B| = {f} nT, evaluation is corrupt" \
      throttle 5
    ###############################################################################
    # Standard AC Ports                                                           #
    ###############################################################################

    time get port timeCaller

    command reg port cmdRegOut
    command recv port cmdIn
    command resp port cmdResponseOut

    text event port logTextOut
    event port logOut
    telemetry port tlmOut

    param get port prmGetOut
    param set port prmSetOut

  }

}
}
