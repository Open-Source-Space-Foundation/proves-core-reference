/**
 * \file AstroLib.hpp
 * \brief F Prime independent astrodynamics kernel.
 *
 * \details
 *  Conventions:
 *      - Distances in km, angles in radians unless suffixed _deg / Au.
 *      - Frames are "of date": MOD / TOD / TEME / ECEF.
 */
#ifndef GNC_ASTROLIB_HPP
#define GNC_ASTROLIB_HPP

#include <cmath>
#include <cstdint>

namespace Gnc {
namespace Astro {

/*
 * ============================================================================
 * Constants
 *
 * References for the values below are documented in Gnc/GNC_CITATIONS.md, in
 * the order they appear here.
 * ============================================================================
 */

constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double DEG2RAD = PI / 180.0;           //!< Degrees -> radians
constexpr double RAD2DEG = 180.0 / PI;           //!< Radians -> degrees
constexpr double ARCSEC2RAD = DEG2RAD / 3600.0;  //!< Arcseconds -> radians

constexpr double JD_J2000 = 2451545.0;       //!< JD of 2000-01-01T12:00:00 TT
constexpr double JD_UNIX_EPOCH = 2440587.5;  //!< JD of 1970-01-01T00:00:00
constexpr double SECONDS_PER_DAY = 86400.0;  //!< SI seconds in a civil day
constexpr double MINUTES_PER_DAY = 1440.0;   //!< Minutes in a civil day
constexpr double DAYS_PER_JCENT = 36525.0;   //!< Days in a Julian century

constexpr double AU_KM = 149597870.7;                         //!< IAU 2012 astronomical unit
constexpr double RADIUS_EARTH_KM = 6378.137;                  //!< WGS-84 equatorial radius
constexpr double WGS_EARTH_FLATTENING = 1.0 / 298.257223563;  //!< WGS-84 flattening
constexpr double RADIUS_SUN_KM = 696000.0;                    //!< Solar radius (Vallado)

//! TT - TAI, fixed by definition.
constexpr double TT_MINUS_TAI_SEC = 32.184;

/**
 * Earth shadow cone half-angles (Vallado, Fundamentals of Astrodynamics,
 * Alg. 34). Constant to good approximation over Earth's orbit.
 */
constexpr double ALPHA_UMB_RAD = 0.264121 * DEG2RAD;
constexpr double ALPHA_PEN_RAD = 0.269007 * DEG2RAD;

/**
 * \brief Unitless F64 3-D vector.
 *
 * \details Carries no frame or unit tag; the caller is responsible for
 * keeping track of both. Every function below that takes a pair of Vec3
 * assumes they share a frame.
 */
struct Vec3 {
    double x;  //!< First component
    double y;  //!< Second component
    double z;  //!< Third component
};

/**
 * \brief Component-wise vector addition.
 * \param a  Left operand
 * \param b  Right operand
 * \return a + b
 */
inline Vec3 vadd(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

/**
 * \brief Component-wise vector subtraction.
 * \param a  Minuend
 * \param b  Subtrahend
 * \return a - b
 */
inline Vec3 vsub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

/**
 * \brief Scale a vector by a scalar.
 * \param a  Vector to scale
 * \param s  Scale factor
 * \return a * s
 */
inline Vec3 vscale(const Vec3& a, double s) {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

/**
 * \brief Euclidean inner product.
 * \param a  Left operand
 * \param b  Right operand
 * \return a . b
 */
inline double vdot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * \brief Right-handed vector cross product.
 * \param a  Left operand
 * \param b  Right operand
 * \return a x b, normal to both and zero when they are parallel
 */
inline Vec3 vcross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
/**
 * \brief Euclidean (L2) norm.
 * \param a  Vector to measure
 * \return |a|, always non-negative
 */
inline double vnorm(const Vec3& a) {
    return std::sqrt(vdot(a, a));
}

/**
 * \brief Normalize a vector to unit length.
 *
 * \details Returns the input unchanged if it is near zero length, so a
 * division by zero never propagates NaNs into the attitude chain.
 * Callers that need to distinguish this case must check the norm
 * themselves.
 *
 * \param a  Vector to normalize
 * \return a / |a|, or a unchanged when |a| < 1e-12
 */
Vec3 vunit(const Vec3& a);

/**
 * \brief Clamp a double to [-1, 1].
 *
 * \details Used before acos/asin to guard against domain errors when
 * rounding pushes a dot product a few ULPs outside the valid range.
 *
 * \param v  Value to clamp
 * \return v limited to [-1, 1]
 */
inline double clampUnit(double v) {
    if (v > 1.0) {
        return 1.0;
    }

    if (v < -1.0) {
        return -1.0;
    }

    return v;
}

/**
 * \brief Spherical linear interpolation between two unit vectors.
 *
 * \details Falls back to a normalized lerp below ~0.1 mrad of
 * separation, where the great-circle arc and the chord agree to within
 * 1e-9 and the sin(omega) denominator would be ill conditioned.
 *
 * \param a  Start direction, assumed unit length
 * \param b  End direction, assumed unit length
 * \param t  Interpolation parameter, 0 gives a and 1 gives b. Not clamped.
 * \return Unit vector along the great circle from a to b
 */
Vec3 vslerp(const Vec3& a, const Vec3& b, double t);

/*
 * ============================================================================
 * Time systems
 * ============================================================================
 */

/**
 * \brief A Julian date carried as two doubles (whole day + fraction).
 *
 * \details A flat double JD near 2.46e6 has ~50 us of resolution left in
 * its mantissa. Splitting the whole-day part from the fraction keeps
 * ~microsecond resolution, which matters for GMST and for time since
 * TLE epoch.
 */
struct JulianDate2 {
    double day;   //!< Large part, nominally a whole/half day number
    double frac;  //!< Small part, nominally in [0, 1)
};

/**
 * \brief All the time scales one cycle of the algorithm needs.
 *
 * \details Produced once per tick by computeTimeScales() and passed
 * downstream, so the clock is read in exactly one place.
 */
struct TimeScales {
    JulianDate2 jdUtc;  //!< Coordinated Universal Time as a split JD
    JulianDate2 jdUt1;  //!< UT1 as a split JD; drives Earth rotation
    double tUt1;        //!< Julian centuries of UT1 since J2000
    double tTt;         //!< Julian centuries of TT  since J2000 (~= TDB)
};

/**
 * \brief Convert POSIX seconds to a split Julian date.
 *
 * \param unixSecondsUtc  Seconds since 1970-01-01T00:00:00Z, UTC, with
 *                        no leap seconds encoded (POSIX convention)
 * \return Split JD with the whole day count in .day and the sub-day
 *         remainder in .frac
 */
JulianDate2 jdFromUnixUtc(double unixSecondsUtc);

/**
 * \brief Collapse a split JD into a single double.
 *
 * \details Only use where a few tens of microseconds of resolution loss
 * is acceptable, e.g. telemetry or the solar model arguments.
 *
 * \param jd  Split Julian date
 * \return jd.day + jd.frac
 */
inline double jdFlatten(const JulianDate2& jd) {
    return jd.day + jd.frac;
}

/**
 * \brief Difference between two split Julian dates, in days.
 *
 * \details Computed part-wise so the ~2.4e6 day offset cancels before
 * the small fractions are combined.
 *
 * \param a  Later date
 * \param b  Earlier date
 * \return a - b in days, negative when a precedes b
 */
inline double jdDiffDays(const JulianDate2& a, const JulianDate2& b) {
    return (a.day - b.day) + (a.frac - b.frac);
}

/**
 * \brief Build every time scale from a UTC POSIX timestamp.
 *
 * \details UT1 drives Earth rotation (GMST, the Sun's mean longitude);
 * TT drives the dynamical arguments (nutation, obliquity, mean anomaly).
 * Both are produced here so no downstream component has to read a clock.
 *
 * \param unixSecondsUtc  Seconds since 1970-01-01T00:00:00Z
 * \param dut1Sec         UT1 - UTC, from IERS Bulletin A. |dut1| < 0.9 s.
 * \param taiMinusUtcSec  Current leap second count (37.0 as of 2017).
 * \return Populated TimeScales for this instant
 */
TimeScales computeTimeScales(double unixSecondsUtc, double dut1Sec, double taiMinusUtcSec);

/**
 * \brief Greenwich Mean Sidereal Time, IAU-1982 model.
 *
 * \details The Earth rotation angle used for every TEME <-> ECEF
 * conversion in the subsystem. Computed once per cycle and shared, so
 * all frames agree even when the clock does not.
 *
 * \param tUt1  Julian centuries of UT1 since J2000
 * \return GMST in radians, wrapped to [0, 2pi)
 */
double gmst1982Rad(double tUt1);

/**
 * \brief Decimal year (e.g. 2026.5772) from a UT1 Julian date.
 *
 * \details Gregorian calendar, with leap years handled exactly: the day
 * count is divided by the true length of that particular year, so the
 * fraction stays continuous across a year boundary.
 *
 * Exists because the WMM's secular-variation terms are a series in
 * decimal year.
 *
 * \param jdUt1  UT1 Julian date, flattened
 * \return Decimal year in the proleptic Gregorian calendar
 */
double decimalYearFromJd(double jdUt1);

/*
 * ============================================================================
 * Precession / nutation
 * ============================================================================
 */

/**
 * \brief Truncated IAU-1980 nutation terms.
 *
 * \details Four largest terms only; good to ~1 arcsec, which is two
 * orders of magnitude finer than the Sun model error.
 */
struct Nutation {
    double dPsi;     //!< Nutation in longitude, rad
    double dEps;     //!< Nutation in obliquity, rad
    double meanEps;  //!< Mean obliquity of the ecliptic, rad
    double trueEps;  //!< True obliquity, rad
    double eqEq;     //!< Equation of the equinoxes, rad
};

/**
 * \brief Evaluate the truncated IAU-1980 nutation series.
 *
 * \param tTt  Julian centuries of TT since J2000
 * \return Nutation angles, obliquities, and the equation of the
 *         equinoxes at that epoch
 */
Nutation nutation1980(double tTt);

/**
 * \brief Rotate the coordinate frame about the 1-axis (Vallado's ROT1).
 *
 * \details Rotates the frame, not the vector. The returned components
 * describe the same physical direction expressed in the rotated frame.
 *
 * \param v         Vector to re-express
 * \param angleRad  Frame rotation angle about the 1-axis, radians
 * \return v expressed in the rotated frame
 */
Vec3 rot1(const Vec3& v, double angleRad);

/**
 * \brief Rotate the coordinate frame about the 3-axis (Vallado's ROT3).
 *
 * \param v         Vector to re-express
 * \param angleRad  Frame rotation angle about the 3-axis, radians
 * \return v expressed in the rotated frame
 */
Vec3 rot3(const Vec3& v, double angleRad);

/**
 * \brief Mean-of-date -> True-of-date (apply nutation).
 *
 * \param v  Vector in MOD
 * \param n  Nutation for the same epoch
 * \return The same direction expressed in TOD
 */
Vec3 modToTod(const Vec3& v, const Nutation& n);

/**
 * \brief True-of-date -> TEME (rotate by the equation of the equinoxes).
 *
 * \param v  Vector in TOD
 * \param n  Nutation for the same epoch
 * \return The same direction expressed in TEME
 */
Vec3 todToTeme(const Vec3& v, const Nutation& n);

/**
 * \brief MOD -> TEME, composing modToTod() and todToTeme().
 *
 * \details This is the frame the whole subsystem works in, because it
 * is what SGP4 produces.
 *
 * \param v  Vector in MOD
 * \param n  Nutation for the same epoch
 * \return The same direction expressed in TEME
 */
Vec3 modToTeme(const Vec3& v, const Nutation& n);

/**
 * \brief TEME -> ECEF (strictly PEF; polar motion is ignored).
 *
 * \details Polar motion is under 0.5 arcsec, far below the SGP4 position
 * error. Invert by calling rot3(v, -gmstRad) with the same angle.
 *
 * \param v        Vector in TEME
 * \param gmstRad  Greenwich Mean Sidereal Time, radians
 * \return The same vector expressed in ECEF
 */
Vec3 temeToEcef(const Vec3& v, double gmstRad);

/**
 * \brief ECEF -> WGS-84 geodetic, Bowring's method (non-iterative).
 *
 * \details One closed-form correction off a parametric-latitude seed.
 * Accurate to well under a meter for any altitude this spacecraft will
 * see. Altitude falls back to |z| - b at the poles, where the p/cos(lat)
 * form is singular.
 *
 * \param rEcefKm  Position in ECEF, kilometers
 * \param latRad   [out] Geodetic latitude, radians
 * \param lonRad   [out] Longitude, radians, from atan2 so in (-pi, pi]
 * \param altKm    [out] Height above the WGS-84 ellipsoid, kilometers
 */
void ecefToGeodetic(const Vec3& rEcefKm, double& latRad, double& lonRad, double& altKm);

/*
 * ============================================================================
 * Solar ephemeris
 * ============================================================================
 */

/**
 * \brief Result of one solar ephemeris evaluation.
 *
 * \details Geocentric: the direction is Earth centre -> Sun. Callers
 * that need the direction from the spacecraft must apply parallax via
 * sunUnitFromSpacecraft().
 */
struct SunState {
    Vec3 unitMod;      //!< Unit vector Earth -> Sun, MOD frame
    double rangeKm;    //!< Earth-Sun distance
    double eclLonRad;  //!< Apparent ecliptic longitude (useful for checkout)
};

/**
 * \brief Low-precision solar ephemeris (Astronomical Almanac /
 *        Vallado Alg. 29).
 *
 * \details Accuracy is roughly 0.01 deg in direction over 1950-2050
 * which is an order of magnitude better than a coarse sun sensor,
 * so it's never the limiting error in the attitude solution.
 *
 * \param tUt1  Julian centuries of UT1 since J2000, for the mean longitude
 * \param tTdb  Julian centuries of TDB (~= TT) since J2000, for the
 *              dynamical arguments
 * \return Sun direction in MOD, range, and apparent ecliptic longitude
 */
SunState sunLowPrecisionMod(double tUt1, double tTdb);

/**
 * ============================================================================
 * Illumination and geometry
 * ============================================================================
 */

/**
 * \brief Where a spacecraft sits relative to Earth's shadow cones.
 *
 * \details Values are fixed and mirrored by Gnc::IlluminationState in
 * FPP; see Gnc::toFpp() in GncConvert.hpp for the mapping.
 */
enum class Illumination : uint8_t {
    SUNLIT = 0,    //!< Full sun, outside the penumbral cone
    PENUMBRA = 1,  //!< Partial occultation, between the two cones
    UMBRA = 2      //!< Total occultation, inside the umbral cone
};

/**
 * \brief Dual-cone Earth shadow model (Vallado Alg. 34).
 *
 * \details Both vectors must be geocentric, in the same frame, in km.
 * The sunward hemisphere short-circuits to SUNLIT before any
 * trigonometry runs.
 *
 * \param rSatKm  Spacecraft position, geocentric, km
 * \param rSunKm  Sun position, geocentric, km (not a unit vector)
 * \return SUNLIT, PENUMBRA, or UMBRA
 */
Illumination shadowConical(const Vec3& rSatKm, const Vec3& rSunKm);

/**
 * \brief Cheaper cylindrical shadow test.
 *
 * \details Models the shadow as a cylinder of Earth's radius rather than
 * two cones, so it has no penumbra. Use where a boolean is enough and
 * the transition regions do not matter.
 *
 * \param rSatKm   Spacecraft position, geocentric, km
 * \param sunUnit  Unit vector toward the Sun, same frame
 * \return true when sunlit, false when eclipsed
 */
bool isSunlitCylindrical(const Vec3& rSatKm, const Vec3& sunUnit);

/**
 * \brief Unit vector from the spacecraft to the Sun, including parallax.
 *
 * \details The parallax correction is at most ~0.0027 deg in LEO, but it's
 * applied anyway because it's one subtraction and it removes a
 * systematic bias from the attitude reference.
 *
 * \param rSatKm   Spacecraft position, geocentric, km
 * \param rSunKm   Sun position, geocentric, km, same frame
 * \param rangeKm  [out] Spacecraft-to-Sun distance, km
 * \return Unit vector spacecraft -> Sun
 */
Vec3 sunUnitFromSpacecraft(const Vec3& rSatKm, const Vec3& rSunKm, double& rangeKm);

/**
 * \brief Beta angle: elevation of the Sun above the orbit plane.
 *
 * \details Positive when the Sun lies on the +h (angular momentum) side.
 * Drives eclipse duration, and the power and thermal profiles.
 *
 * \param rKm      Spacecraft position, km
 * \param vKmS     Spacecraft velocity, km/s, same frame as rKm
 * \param sunUnit  Unit vector toward the Sun, same frame
 * \return Beta angle in radians, in [-pi/2, pi/2]
 */
double betaAngleRad(const Vec3& rKm, const Vec3& vKmS, const Vec3& sunUnit);

/**
 * \brief Nadir unit vector (spacecraft -> Earth centre).
 *
 * \param rKm  Spacecraft position, geocentric
 * \return Unit vector pointing at the Earth centre
 */
inline Vec3 nadirUnit(const Vec3& rKm) {
    return vunit(vscale(rKm, -1.0));
}

}  // namespace Astro
}  // namespace Gnc

#endif  // GNC_ASTROLIB_HPP
