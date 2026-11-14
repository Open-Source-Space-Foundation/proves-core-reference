/**
 * \file AstroLib.cpp
 * \brief Implementation of the astrodynamics kernel.
 */
#include "AstroLib.hpp"

namespace Gnc {
namespace Astro {

namespace {

/**
 * Shadow-cone geometry, two constants per cone.
 *
 * Vallado writes the penumbra/umbra vertical extents as
 *     penVert = tan(a_pen) * (R_E/sin(a_pen) + satHoriz)
 *     umbVert = tan(a_umb) * (R_E/sin(a_umb) - satHoriz)
 * The R_E/sin(a) terms are ~1.36e6 km, so evaluating them literally throws
 * away significant digits. Distributing tan() through gives the equivalent and better conditioned
 *     penVert = R_E/cos(a_pen) + tan(a_pen)*satHoriz
 *     umbVert = R_E/cos(a_umb) - tan(a_umb)*satHoriz
 */
const double kTanAlphaUmb = std::tan(ALPHA_UMB_RAD);
const double kTanAlphaPen = std::tan(ALPHA_PEN_RAD);
const double kSecRUmbKm = RADIUS_EARTH_KM / std::cos(ALPHA_UMB_RAD);
const double kSecRPenKm = RADIUS_EARTH_KM / std::cos(ALPHA_PEN_RAD);

}  // namespace

/*
 * ============================================================================
 * Vector helpers
 * ============================================================================
 */

Vec3 vunit(const Vec3& a) {
    const double n = vnorm(a);
    if (n < 1.0e-12) {
        return a;
    }
    return vscale(a, 1.0 / n);
}

Vec3 vslerp(const Vec3& a, const Vec3& b, double t) {
    const double c = clampUnit(vdot(a, b));
    const double omega = std::acos(c);
    /*
     * Below ~0.1 mrad the great-circle arc and the chord differ by less
     * than 1e-9, so normalized lerp works for our purposes.
     */
    if (omega < 1.0e-4) {
        return vunit(vadd(vscale(a, 1.0 - t), vscale(b, t)));
    }
    const double so = std::sin(omega);
    return vadd(vscale(a, std::sin((1.0 - t) * omega) / so), vscale(b, std::sin(t * omega) / so));
}

/*
 * ============================================================================
 * Time systems
 * ============================================================================
 */

JulianDate2 jdFromUnixUtc(double unixSecondsUtc) {
    /*
     * Split so that .day carries whole days from the Unix epoch and .frac
     * carries the sub-day remainder.
     */
    const double days = unixSecondsUtc / SECONDS_PER_DAY;
    const double whole = std::floor(days);
    JulianDate2 jd;
    jd.day = JD_UNIX_EPOCH + whole;
    jd.frac = days - whole;
    return jd;
}

TimeScales computeTimeScales(double unixSecondsUtc, double dut1Sec, double taiMinusUtcSec) {
    TimeScales ts;

    ts.jdUtc = jdFromUnixUtc(unixSecondsUtc);

    /*
     * UT1 = UTC + (UT1-UTC). Drives Earth rotation: GMST and the Sun's
     * mean longitude.
     */
    ts.jdUt1 = ts.jdUtc;
    ts.jdUt1.frac = ts.jdUtc.frac + dut1Sec / SECONDS_PER_DAY;

    /*
     * TT = TAI + 32.184 = UTC + (TAI-UTC) + 32.184. Drives the dynamical
     * arguments: nutation, obliquity, the Sun's mean anomaly.
     */
    const double ttOffsetDays = (taiMinusUtcSec + TT_MINUS_TAI_SEC) / SECONDS_PER_DAY;

    /*
     * Compute centuries part-wise so the 2.4e6 day offset is subtracted
     * before the small fraction is added.
     */
    ts.tUt1 = ((ts.jdUt1.day - JD_J2000) + ts.jdUt1.frac) / DAYS_PER_JCENT;
    ts.tTt = ((ts.jdUtc.day - JD_J2000) + (ts.jdUtc.frac + ttOffsetDays)) / DAYS_PER_JCENT;

    return ts;
}

double gmst1982Rad(double tUt1) {
    const double t2 = tUt1 * tUt1;

    /*
     * 876600 h * 3600 s/h = 3155760000 s. Reduce this dominant term mod one
     * day *before* summing, otherwise at T ~ 0.26 the product is ~8e8 s and
     * the 1e-4 s terms fall off the bottom of the mantissa.
     */
    double gmstSec = 67310.54841 + std::fmod(3155760000.0 * tUt1, SECONDS_PER_DAY) + 8640184.812866 * tUt1 +
                     0.093104 * t2 - 6.2e-6 * t2 * tUt1;

    // 1 second of sidereal time = 1/240 degree.
    double rad = std::fmod(gmstSec * (1.0 / 240.0) * DEG2RAD, TWO_PI);
    if (rad < 0.0) {
        rad += TWO_PI;
    }
    return rad;
}

namespace {

/**
 * Julian date at 0h UT of a Gregorian calendar date.
 * (Fliegel & Van Flandern)
 */
double jdAtMidnight(long year, int month, int day) {
    const long a = (14 - month) / 12;
    const long y = year + 4800 - a;
    const long m = month + 12 * a - 3;
    const long jdn = static_cast<long>(day) + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    return static_cast<double>(jdn) - 0.5;
}

}  // namespace

double decimalYearFromJd(double jdUt1) {
    // Inverse Fliegel & Van Flandern, to recover the calendar year.
    const long jdn = static_cast<long>(std::floor(jdUt1 + 0.5));

    long l = jdn + 68569;
    const long n = (4 * l) / 146097;
    l -= (146097 * n + 3) / 4;
    const long i = (4000 * (l + 1)) / 1461001;
    l = l - (1461 * i) / 4 + 31;
    const long j = (80 * l) / 2447;
    l = j / 11;
    const long year = 100 * (n - 49) + i + l;

    /*
     * Divide by the actual length of THIS year, so 2028 gets 366 days
     * and the fraction stays continuous across the boundary.
     */
    const double jdStart = jdAtMidnight(year, 1, 1);
    const double jdEnd = jdAtMidnight(year + 1, 1, 1);

    return static_cast<double>(year) + (jdUt1 - jdStart) / (jdEnd - jdStart);
}

/*
 * ============================================================================
 * Nutation
 * ============================================================================
 */

Nutation nutation1980(double tTt) {
    const double t = tTt;
    const double t2 = t * t;
    const double t3 = t2 * t;

    // Mean longitude of the Moon's ascending node.
    const double om = (125.04452222 - 1934.136261 * t + 0.0020708 * t2 + t3 / 450000.0) * DEG2RAD;
    // Mean longitude of the Sun.
    const double L = (280.4665 + 36000.7698 * t) * DEG2RAD;
    // Mean longitude of the Moon.
    const double Lp = (218.3165 + 481267.8813 * t) * DEG2RAD;

    const double s2L = std::sin(2.0 * L);
    const double s2Lp = std::sin(2.0 * Lp);
    const double sOm = std::sin(om);
    const double s2Om = std::sin(2.0 * om);

    // Four dominant IAU-1980 terms, in arcseconds.
    const double dPsiAs = -17.20 * sOm - 1.32 * s2L - 0.23 * s2Lp + 0.21 * s2Om;
    const double dEpsAs =
        9.20 * std::cos(om) + 0.57 * std::cos(2.0 * L) + 0.10 * std::cos(2.0 * Lp) - 0.09 * std::cos(2.0 * om);

    Nutation n;
    n.dPsi = dPsiAs * ARCSEC2RAD;
    n.dEps = dEpsAs * ARCSEC2RAD;
    n.meanEps = (23.439291111 - 0.0130041667 * t - 1.64e-7 * t2 + 5.04e-7 * t3) * DEG2RAD;
    n.trueEps = n.meanEps + n.dEps;

    // Equation of the equinoxes, with the two 1997 kinematic corrections.
    n.eqEq = n.dPsi * std::cos(n.meanEps) + 0.00264 * ARCSEC2RAD * sOm + 0.000063 * ARCSEC2RAD * s2Om;

    return n;
}

/*
 * ============================================================================
 * Frame rotations
 * ============================================================================
 */

Vec3 rot1(const Vec3& v, double a) {
    const double c = std::cos(a);
    const double s = std::sin(a);
    return Vec3{v.x, c * v.y + s * v.z, -s * v.y + c * v.z};
}

Vec3 rot3(const Vec3& v, double a) {
    const double c = std::cos(a);
    const double s = std::sin(a);
    return Vec3{c * v.x + s * v.y, -s * v.x + c * v.y, v.z};
}

Vec3 modToTod(const Vec3& v, const Nutation& n) {
    // r_TOD = ROT1(-trueEps) * ROT3(-dPsi) * ROT1(meanEps) * r_MOD
    return rot1(rot3(rot1(v, n.meanEps), -n.dPsi), -n.trueEps);
}

Vec3 todToTeme(const Vec3& v, const Nutation& n) {
    // r_TEME = ROT3(-eqEq) * r_TOD
    return rot3(v, -n.eqEq);
}

Vec3 modToTeme(const Vec3& v, const Nutation& n) {
    return todToTeme(modToTod(v, n), n);
}

Vec3 temeToEcef(const Vec3& v, double gmstRad) {
    return rot3(v, gmstRad);
}

void ecefToGeodetic(const Vec3& r, double& latRad, double& lonRad, double& altKm) {
    const double a = RADIUS_EARTH_KM;
    const double f = WGS_EARTH_FLATTENING;
    const double b = a * (1.0 - f);
    const double e2 = f * (2.0 - f);
    const double ep2 = e2 / (1.0 - e2);

    const double p = std::sqrt(r.x * r.x + r.y * r.y);
    lonRad = std::atan2(r.y, r.x);

    // Bowring's parametric latitude seed, then one closed-form correction.
    const double th = std::atan2(r.z * a, p * b);
    const double st = std::sin(th);
    const double ct = std::cos(th);
    latRad = std::atan2(r.z + ep2 * b * st * st * st, p - e2 * a * ct * ct * ct);

    const double sl = std::sin(latRad);
    const double cl = std::cos(latRad);
    const double N = a / std::sqrt(1.0 - e2 * sl * sl);

    if (std::fabs(cl) > 1.0e-6) {
        altKm = p / cl - N;
    } else {
        // Polar singularity: p/cos(lat) blows up.
        altKm = std::fabs(r.z) - b;
    }
}

/*
 * ============================================================================
 * Solar ephemeris
 * ============================================================================
 */

SunState sunLowPrecisionMod(double tUt1, double tTdb) {
    /*
     * 1. Mean longitude of the Sun, referred to the mean equinox of date.
     *    Driven by UT1 in the classical formulation.
     */
    const double lambdaM = (280.460 + 36000.771 * tUt1) * DEG2RAD;

    // 2. Mean anomaly of the Sun. A dynamical argument, so TDB (~= TT).
    const double M = (357.5291092 + 35999.05034 * tTdb) * DEG2RAD;

    const double sM = std::sin(M);
    const double s2M = std::sin(2.0 * M);
    const double cM = std::cos(M);
    const double c2M = std::cos(2.0 * M);

    /*
     * 3. Equation of centre: two-term expansion of Kepler's equation for
     *    e = 0.0167. Converts mean longitude to true ecliptic longitude.
     */
    const double lambdaEcl = lambdaM + (1.914666471 * sM + 0.019994643 * s2M) * DEG2RAD;

    // 4. Radius vector from the same expansion, in AU.
    const double rAu = 1.000140612 - 0.016708617 * cM - 0.000139589 * c2M;

    // 5. Mean obliquity of the ecliptic.
    const double eps = (23.439291 - 0.0130042 * tTdb) * DEG2RAD;

    /*
     * 6. Rotate the ecliptic-plane unit vector (cos L, sin L, 0) about the
     *    1-axis by -eps to land in the equatorial MOD frame.
     */
    const double sl = std::sin(lambdaEcl);
    SunState out;
    out.unitMod.x = std::cos(lambdaEcl);
    out.unitMod.y = std::cos(eps) * sl;
    out.unitMod.z = std::sin(eps) * sl;
    out.rangeKm = rAu * AU_KM;
    out.eclLonRad = std::fmod(lambdaEcl, TWO_PI);

    return out;
}

/*
 * ============================================================================
 * Illumination and geometry
 * ============================================================================
 */

Illumination shadowConical(const Vec3& rSatKm, const Vec3& rSunKm) {
    // Sunward hemisphere is always lit;
    if (vdot(rSatKm, rSunKm) >= 0.0) {
        return Illumination::SUNLIT;
    }

    const Vec3 sHat = vunit(rSunKm);
    const double rMag2 = vdot(rSatKm, rSatKm);

    // Distance along the anti-sun axis, and perpendicular offset from it.
    const double satHoriz = -vdot(sHat, rSatKm);
    double perp2 = rMag2 - satHoriz * satHoriz;
    if (perp2 < 0.0) {
        perp2 = 0.0;
    }
    const double satVert = std::sqrt(perp2);

    const double penVert = kSecRPenKm + kTanAlphaPen * satHoriz;
    if (satVert > penVert) {
        return Illumination::SUNLIT;
    }

    const double umbVert = kSecRUmbKm - kTanAlphaUmb * satHoriz;
    if (satVert <= umbVert) {
        return Illumination::UMBRA;
    }
    return Illumination::PENUMBRA;
}

bool isSunlitCylindrical(const Vec3& rSatKm, const Vec3& sunUnit) {
    const double along = vdot(rSatKm, sunUnit);
    if (along >= 0.0) {
        return true;
    }
    const double perp2 = vdot(rSatKm, rSatKm) - along * along;
    return perp2 > (RADIUS_EARTH_KM * RADIUS_EARTH_KM);
}

Vec3 sunUnitFromSpacecraft(const Vec3& rSatKm, const Vec3& rSunKm, double& rangeKm) {
    const Vec3 d = vsub(rSunKm, rSatKm);
    rangeKm = vnorm(d);
    return vunit(d);
}

double betaAngleRad(const Vec3& rKm, const Vec3& vKmS, const Vec3& sunUnit) {
    const Vec3 h = vunit(vcross(rKm, vKmS));

    /*
     * beta = 90deg - angle(h, s). The asin(h.s) form loses ~8 digits
     * near beta = +/-90 because asin has an infinite derivative at 1. The
     * atan2 form is well conditioned across the whole range.
     */
    const double cosTheta = vdot(h, sunUnit);
    const double sinTheta = vnorm(vcross(h, sunUnit));
    return std::atan2(cosTheta, sinTheta);
}

}  // namespace Astro
}  // namespace Gnc
