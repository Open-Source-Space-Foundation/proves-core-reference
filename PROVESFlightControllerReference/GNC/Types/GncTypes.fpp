# ======================================================================
# GncTypes.fpp
#
# Shared data types for the Guidance, Navigation, and Control (GNC) subsystem.
#
# ----------------------------------------------------------------------
#   Naming Conventions
#
#   1. Units and frames are included in the name:
#        <quantity><Frame><Unit>
#      e.g. posTemeKm, velTemeKmS, gmstRad, sunRangeKm, altKm.
#      Omit the unit when the value dimensionless(sunUnitTeme).
#      Omit the frame only when it doesn't vary for that quantity.
#
#      Keep names the same when they cross boundaries:
#      posTemeKm in a struct should be posTemeKm in every function that
#      handles it.
#
#   2. Angles use radians in all port payloads. Only use degrees in telemetry.
#
#   3. Payload nouns have a specific meaning and are not interchangeable:
#        State    - a measured or propagated condition of the satellite
#        Sample   - one observation from a sensor or a model
#        Solution - an estimate produced by solving something
#
#   4. Push port types end in Send. The suffix describes the port, not
#      the payload.
# ======================================================================

module Gnc {

  # --------------------------------------------------------------------
  # Frames
  # --------------------------------------------------------------------

  @ Identifies the reference frame a vector is expressed in.
  @
  @ TEME is the system-wide inertial frame because it's what SGP4
  @ produces.
  @ The Sun model (MOD) and the magnetic model (ECEF) both rotate into
  @ TEME at their own boundary.
  @
  enum FrameId: U8 {
    @ Unset. Treated as a fault by consumers.
    UNKNOWN = 0
    @ True Equator, Mean Equinox of date. The system inertial frame.
    TEME    = 1
    @ Mean Equator, Mean Equinox of date. Solar model output only.
    MOD     = 2
    @ True Equator, True Equinox of date. Intermediate only.
    TOD     = 3
    @ Earth-fixed (ITRS/PEF, polar motion neglected). WMM input only.
    ECEF    = 4
    @ Mean equator and equinox of J2000.0. Currently unused.
    J2000   = 5
    @ Spacecraft body frame. Sensor outputs.
    BODY    = 6
  }

  # --------------------------------------------------------------------
  # Vectors
  # Two types: Vec3d (F64) and Vec3f (F32), precision is the only difference.
  # SCALAR has a single precision FPU, so Vec3f is used whenever double precision isn't necessary.
  #
  #   Current Usage:
  #   Vec3d: The orbit domain. SGP4 needs double precision.
  #
  #   Vec3f: The attitude and magnetic field domains.
  # --------------------------------------------------------------------

  @ Cartesian 3-vector, F64. Units and frame are named by the field
  @ that holds it.
  struct Vec3d {
    x: F64
    y: F64
    z: F64
  } default { x = 0.0, y = 0.0, z = 0.0 }

  @ Cartesian 3-vector, F32. Units and frame are named by the field
  @ that holds it.
  struct Vec3f {
    x: F32 format "{.6f}"
    y: F32 format "{.6f}"
    z: F32 format "{.6f}"
  } default { x = 0.0, y = 0.0, z = 0.0 }

  # --------------------------------------------------------------------
  # Vector observations
  # --------------------------------------------------------------------

  @ One vector observation from a sensor or model, ex: magnetometer, WMM
  @
  @ Not required to be a unit vector. Consumers that need unit vectors
  @ must normalize the sample.
  struct VectorSample {
    @ The vector. Quantity and units kept in the port name.
    vec: Vec3f
    @ Which frame vec is expressed in.
    frame: FrameId
    @ When the observation was taken.
    stamp: Fw.Time
    @ Is the sample valid?
    valid: bool
  }

  @ Push port for vector producers e.g. sun sensor, magnetometer,
  @ solar ephemeris, magnetic field model.
  port VectorSampleSend(ref sample: VectorSample)

  # --------------------------------------------------------------------
  # Orbit
  # --------------------------------------------------------------------

  @ Why the orbit state is / isn't usable. Used for diagnostic detail.
  enum OrbitValidity: U8 {
    @ No TLE has been loaded since boot. Time and GMST are still valid.
    NO_TLE      = 0
    @ Time source is not usable. NOTHING in the state is valid.
    NO_TIME     = 1
    @ SGP4 returned a non-zero error code
    PROP_ERROR  = 2
    @ Solution is good but the TLE is older than MAX_TLE_AGE_DAYS
    STALE       = 3
    @ Fully valid
    VALID       = 4
  }

  @ Spacecraft orbit state for one cycle, TEME Frame.
  struct OrbitState {
    @ Diagnostic reason For events and telemetry.
    validity: OrbitValidity

    @ True when stamp, jdUt1, jdTt and gmstRad are valid.
    timeUsable: bool

    @ True when posTemeKm, velTemeKmS, latRad, lonRad, altKm and
    @ tleAgeDays are valid.
    positionUsable: bool

    @ Wall-clock instant this state describes.
    @
    @ Carried so consumers can stamp their outputs with
    @ the observation epoch.
    stamp: Fw.Time

    @ UT1 Julian date. Drives Earth rotation and the Sun's mean
    @ longitude.
    jdUt1: F64

    @ Terrestrial Time Julian date. Drives the dynamical arguments:
    @ nutation, obliquity, the Sun's mean anomaly.
    @
    @ Carried for solar ephemeris.
    jdTt: F64

    @ Greenwich Mean Sidereal Time at jdUt1, radians, [0, 2pi).
    @
    gmstRad: F64

    @ Spacecraft position, TEME, kilometers
    posTemeKm: Vec3d

    @ Spacecraft velocity, TEME, kilometers per second
    velTemeKmS: Vec3d

    @ Sub-satellite geodetic latitude, radians (WGS-84)
    @
    latRad: F64

    @ Sub-satellite geodetic longitude, radians (WGS-84)
    lonRad: F64

    @ Geodetic altitude above the WGS-84 ellipsoid, kilometers
    altKm: F64

    @ Age of the loaded TLE, days. SGP4 degrades roughly 1-3 km/day.
    tleAgeDays: F32
  }

  @ Broadcasts the orbit state.
  port OrbitStateSend(ref orbitState: OrbitState)

  # --------------------------------------------------------------------
  # Solar geometry
  # --------------------------------------------------------------------

  @ Illumination state with respect to the Earth's shadow
  enum IlluminationState: U8 {
    SUNLIT   = 0
    PENUMBRA = 1
    UMBRA    = 2
  }

  @ Why the solar state is / isn't usable. Diagnostic detail.e.
  enum SolarValidity: U8 {
    @ No usable time, so nothing could be computed
    NO_TIME    = 0
    @ Direction computed, but no orbit was available: the vector is
    @ geocentric and shadow / beta are unavailable
    GEOCENTRIC = 1
    @ Fully valid
    VALID      = 2
  }

  @ Solar geometry for one cycle.
  struct SolarState {
    @ Diagnostic reason. For events and telemetry.
    validity: SolarValidity

    @ True when sunUnitTeme and sunRangeKm are valid.
    directionUsable: bool

    @ True when illumination and betaRad are valid. Implies
    @ directionUsable.
    geometryUsable: bool

    @ Unit vector toward the Sun, TEME. From the spacecraft when
    @ geometryUsable, otherwise from the Earth's center.
    @ Difference is at most 0.0027 deg in LEO << the solar model's 0.01 deg precision.
    @ Usable for attitude determination in either case.
    sunUnitTeme: Vec3f

    @ Range to the Sun, kilometers
    sunRangeKm: F64

    @ Shadow state
    illumination: IlluminationState

    @ Sun elevation above the orbit plane, RADIANS.
    betaRad: F64
  }

  @ Broadcasts solar geometry.
  port SolarStateSend(ref solarState: SolarState)

}
