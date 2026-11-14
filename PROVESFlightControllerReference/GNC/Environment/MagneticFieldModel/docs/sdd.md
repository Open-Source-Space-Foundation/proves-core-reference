# Gnc::Environment::MagneticFieldModel

WMM2025 evaluated at the propagated orbit position. Passive, driven by the arrival of `Gnc::OrbitState`. The rate group tick is telemetry only.

## Requirements

| Name | Description | Validation Method | Level | Pass Criteria | Status | Reason |
|---|---|---|---|---|---|---|
|MAG0001|The MagneticFieldModel shall evaluate the field once per arrival of an OrbitState|Unit Test|Host|Three `orbitIn` arrivals trigger three `fieldOut` invocations.|||
|MAG0002|The MagneticFieldModel shall not recompute the field on a rate group tick|Unit Test|Host|After one arrival, ten `run` ticks leave `FieldTemeNt` and `FieldMagnitudeNt` unchanged and produce no `fieldOut` invocations|||
|MAG0003|The MagneticFieldModel shall reject an evaluation when the OrbitState carries no usable position|Unit Test|Host|`orbitIn` with `positionUsable = false` emits `OrbitUnusable` carrying the orbit validity, publishes `fieldOut` with `valid = false`, and increments `FieldRejectCount`|||
|MAG0004|The MagneticFieldModel shall reject an evaluation above the WMM validated altitude|Unit Test|Host|`evaluateField` at 20000 km returns `ALTITUDE_OUT_OF_RANGE`; the component emits `EvaluationOutOfRange` with the altitude and decimal year|||
|MAG0005|The MagneticFieldModel shall reject an evaluation below the WMM validated altitude|Unit Test|Host|`evaluateField` at -50 km returns `ALTITUDE_OUT_OF_RANGE`|||
|MAG0006|The MagneticFieldModel shall accept an evaluation just inside each altitude bound|Unit Test|Host|`evaluateField` at 849 km and at -0.5 km both return `OK`|||
|MAG0007|The MagneticFieldModel shall reject an evaluation outside the WMM2025 coefficient epoch window|Unit Test|Host|`evaluateField` at JD 2451545.0 (year 2000) returns `EPOCH_OUT_OF_RANGE`|||
|MAG0008|The MagneticFieldModel shall report the decimal year it attempted even when the evaluation is rejected|Unit Test|Host|After an `EPOCH_OUT_OF_RANGE` rejection, `FieldSolution::decYear` is within [1999.0, 2001.0] and `EvalDecYear` telemetry reports it|||
|MAG0009|The MagneticFieldModel shall reject a field magnitude too small to be physical|Unit Test|Host|`evaluateField` returning under 1 nT yields `DEGENERATE_FIELD`; the component emits `FieldDegenerate` at warning high, publishes `valid = false`, and does not emit `EvaluationOutOfRange`|||
|MAG0010|The MagneticFieldModel reject a non-finite field|Unit Test|Host|A NaN injected into the field vector produces `DEGENERATE_FIELD`|||
|MAG0011|The MagneticFieldModel shall republish the last good field, flagged invalid, on every rejection path|Unit Test|Host|After one successful evaluation followed by any rejection, `fieldOut` carries the previous field vector with `valid = false`|||
|MAG0012|The MagneticFieldModel shall publish the full field vector in nanotesla, not a unit vector|Unit Test|Host|magnitude of `fieldOut.vec` is in the thousands of nT, not 1.0|||
|MAG0013|The MagneticFieldModel shall emit FieldRestored only on the recovery edge|Unit Test|Host|Valid, then three rejections, then valid: exactly one `FieldRestored`. A first-ever successful evaluation emits none|||
|MAG0014|The field magnitude shall fall within the LEO envelope|Oracle Test|Host|At 420 km on an ISS-like position in 2026, magnitude of B is between 18000 and 60000 nT|||
|MAG0015|The field shall match published WMM2025 test values|Oracle Test|Host|At the NOAA WMM2025 test-value points, computed X, Y, Z components agree with the published table within 1 nT|||
|MAG0016|The kilometer to meter conversion shall be confined to a single site|Inspection|Host|`KM_TO_M` appears exactly once in `WmmModel.cpp`||||
|MAG0017|The MagneticFieldModel shall broadcast the field to every connected output port instance|Unit Test|Host|With all `fieldOut` instances connected, one arrival produces one invocation per instance carrying identical payloads|||
|MAG0018|The MagneticFieldModel shall track the field along a real orbit|Integration Test|Board|With a current LEO TLE loaded, over one orbit `FieldMagnitudeNt` varies smoothly between roughly 20000 and 50000 nT with no discontinuity above 2000 nT between consecutive 1 Hz samples|||

## Port Descriptions

| Name | Type | Description |
|---|---|---|
|orbitIn|Gnc.OrbitStateSend (guarded)|Orbit state from OrbitPropagator. Arrival triggers an evaluation.|
|fieldOut|Gnc.VectorSampleSend|Magnetic field vector with magnitude, TEME, nT|
|run|Svc.Sched (guarded)|Rate group tick, telemetry only.|
|cmdIn / cmdRegOut / cmdResponseOut|Fw.Cmd*|Command dispatch|
|logOut / logTextOut|Fw.Log*|Events|
|tlmOut|Fw.Tlm|Telemetry|
|timeCaller|Fw.Time|Event and telemetry stamping only|
|prmGetOut / prmSetOut|Fw.Prm*|Parameters|

Input ports are guarded because evaluation runs on the propagator's thread while telemetry runs on the rate group thread.

## Parameters

None.

| Constant | Value | Description |
|---|---|---|
|minYear / maxYear|2025.0 / 2030.0|WMM2025 validated epoch window|
|minAltKm / maxAltKm|-1.0 / 850.0|WMM validated altitude range above the WGS-84 ellipsoid|
|minMagnitudeNt|1.0|Degeneracy threshold. Far below any real geomagnetic field.|

## Commands

None.

## Events

| Name | ID | Severity | Throttle | Description |
|---|---|---|---|---|
|EvaluationOutOfRange|0x00|warning low|5|Query outside the validated altitude or epoch range|
|OrbitUnusable|0x01|warning low|5|Orbit state carries no usable position|
|FieldRestored|0x02|activity high|-|Evaluation recovered after a dropout|
|FieldDegenerate|0x03|warning high|5|Magnitude too small to be physical, bad evaluation.|

## Telemetry

| Name | ID | Type | Description |
|---|---|---|---|
|FieldTemeNt|0x00|Gnc.Vec3f|Last computed field vector, TEME, nT|
|FieldMagnitudeNt|0x01|F32|Field magnitude, nT|
|EvalDecYear|0x02|F32|Decimal year the model was last evaluated at|
|FieldValid|0x03|bool|Whether the last evaluation produced a usable field|
|FieldRejectCount|0x04|U32|Cumulative rejected evaluations, any reason|

## Component States

| Name | Description |
|---|---|
|m_lastField|`FieldSolution`. Most recent successful evaluation, republished flagged invalid on rejection paths.|
|m_lastDecYear|Decimal year of the most recent evaluation.|
|m_lastValid|Whether the most recent evaluation succeeded.|
|m_everValid|Whether any evaluation has ever succeeded. Suppresses `FieldRestored` before the first success.|
|m_rejectCount|Cumulative rejected evaluations.|

## Design

### Diagrams

Add sequence and state diagrams here.

## Change Log

| Date | Description |
|---|---|
| TBD | Initial draft |
