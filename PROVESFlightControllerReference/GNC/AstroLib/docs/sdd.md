# Gnc::Astro (AstroLib)

Shared astrodynamics library for the GNC subsystem. Time scales, frame rotations, geodetic conversion, solar model, and eclipse geometry.
## Requirements

| Name | Description | Method | Level | Pass Criteria | Status | Reason |
|---|---|---|---|---|---|---|
|AST0001|AstroLib shall convert POSIX seconds to a split Julian date without losing sub-second precision|Oracle Test|Host|`jdFromUnixUtc` at 2000-01-01T12:00:00Z returns `day + frac` equal to 2451545.0 within 1e-9|||
|AST0002|AstroLib shall derive UT1 and TT from UTC|Oracle Test|Host|With `dut1 = 0.3` and `leapSec = 37`, UT1 minus UTC is 0.3 s and TT minus UTC is 69.184 s, each within 1e-6 s|||
|AST0003|AstroLib shall compute GMST to published accuracy at J2000|Oracle Test|Host|`gmst1982Rad(0.0)` equals 280.46061837 deg within 1e-8 deg|||
|AST0004|AstroLib shall compute GMST correctly away from J2000|Oracle Test|Host|GMST advances 360.9856473 deg per UT1 day within 1e-6 deg, checked across at least three epochs spanning a decade|||
|AST0005|GMST shall be wrapped into `[0, 2*pi)`|Unit Test|Host|Evaluated at 1000 epochs spanning 2000 to 2050, the result is never negative and never reaches `2*pi`|||
|AST0006|AstroLib shall convert a Julian date to a decimal year|Oracle Test|Host|JD 2451545.0 gives 2000.0013 within 1e-4 years; 2024-12-31T23:59:59Z gives just under 2025.0; 2024-02-29 is handled as a leap day|||
|AST0007|AstroLib shall compute the 1980 nutation series|Oracle Test|Host|Mean obliquity at J2000 is 23.4392911 deg within 1e-7 deg; at a second epoch, `dPsi` and `dEps` agree with a published table within 0.1 arcsec|||
|AST0008|Frame rotations shall preserve vector length|Unit Test|Host|`rot1`, `rot3`, `modToTod`, `todToTeme` and `modToTeme` each preserve `vnorm` within 1e-12 relative, for 100 random vectors and angles|||
|AST0009|The TEME to ECEF rotation shall be exactly invertible|Unit Test|Host|`rot3(temeToEcef(v, g), -g)` reproduces `v` within 1e-9 km for LEO-magnitude vectors|||
|AST0010|AstroLib shall convert ECEF to WGS-84 geodetic coordinates|Oracle Test|Host|At least six reference points spanning the equator, mid-latitudes near 45 deg, high latitude, both poles, and a negative altitude reproduce published latitude and longitude within 1e-7 deg and altitude within 1e-3 km|||
|AST0011|The geodetic conversion shall round trip|Unit Test|Host|Converting geodetic to ECEF (computed independently in the test) and back reproduces the input within 1e-7 deg and 1e-3 km, for 200 points sampled over the sphere and -100 to 1000 km altitude|||
|AST0012|The solar model shall place the Sun correctly at the solstices and equinoxes|Oracle Test|Host|2026 declination is +23.44 deg at the June solstice and -23.44 deg at the December solstice, within 0.02 deg; within 0.02 deg of zero at both equinoxes|||
|AST0013|The solar model shall reproduce the annual Earth-Sun range variation|Oracle Test|Host|Range is minimum in early January at 147.10e6 km and maximum in early July at 152.10e6 km, each within 0.05e6 km|||
|AST0014|The conical shadow model shall classify umbra, penumbra and sunlit correctly|Oracle Test|Host|A point directly behind the Earth at LEO altitude is `UMBRA`; a point on the sunward side is `SUNLIT`; sweeping the terminator produces a `PENUMBRA` band whose angular width matches the geometric prediction within 10 percent|||
|AST0015|The cylindrical and conical shadow models shall agree away from the penumbra|Unit Test|Host|Over 1000 sampled positions, `isSunlitCylindrical` disagrees with `shadowConical != UMBRA` only where the conical model reports `PENUMBRA`|||
|AST0016|The spacecraft-centred Sun vector shall differ from the geocentric one by the expected parallax|Oracle Test|Host|At 400 km altitude the angle is non-zero and below 0.0027 deg; at geostationary altitude it is roughly 0.016 deg|||
|AST0017|The beta angle shall follow a documented sign convention|Oracle Test|Host|Orbit angular momentum aligned with the Sun gives +90 deg; reversed velocity gives -90 deg; a Sun vector in the orbit plane gives 0 deg; an off-axis case matches a hand computation within 1e-6 deg|||
|AST0018|Spherical interpolation shall reproduce its endpoints and stay on the unit sphere|Unit Test|Host|`vslerp(a, b, 0)` equals `a` and `vslerp(a, b, 1)` equals `b` within 1e-12; `vnorm` is 1.0 within 1e-12 at 100 interior values of t|||
|AST0019|Spherical interpolation shall behave predictably for near-parallel and near-antiparallel inputs|Unit Test|Host|With `a` and `b` separated by 1e-9 rad the result is finite and unit length; with them antiparallel the result is finite, unit length, and documented as an arbitrary great circle rather than NaN|||
|AST0020|Vector normalization shall reject degenerate input rather than returning NaN|Unit Test|Host|`vunit` of a zero vector, a NaN vector, and an Inf vector each return a documented sentinel rather than propagating a non-finite value|||

## Public Methods

| Group | Functions |
|---|---|
|Vector algebra|`vadd`, `vsub`, `vscale`, `vdot`, `vcross`, `vnorm`, `vunit`, `clampUnit`, `vslerp`, `nadirUnit`|
|Julian dates|`jdFromUnixUtc`, `jdFlatten`, `jdDiffDays`|
|Time scales|`computeTimeScales`, `gmst1982Rad`, `decimalYearFromJd`|
|Precession and nutation|`nutation1980`|
|Frame rotations|`rot1`, `rot3`, `modToTod`, `todToTeme`, `modToTeme`, `temeToEcef`|
|Geodesy|`ecefToGeodetic`|
|Solar|`sunLowPrecisionMod`, `sunUnitFromSpacecraft`|
|Eclipse and geometry|`shadowConical`, `isSunlitCylindrical`, `betaAngleRad`|

## Types

| Name | Description |
|---|---|
|`Vec3`|Cartesian 3-vector, F64. Units and frame are named by the variable holding it.|
|`JulianDate2`|Split Julian date, `day` plus `frac`.|
|`TimeScales`|UTC, UT1 and TT for one instant, as both split Julian dates and centuries from J2000.|
|`Nutation`|`dPsi`, `dEps`, mean and true obliquity, equation of the equinoxes.|
|`SunState`|Unit vector in MOD plus range in km.|
|`Illumination`|`SUNLIT`, `PENUMBRA`, `UMBRA`.|

## Constants

| Name | Value | Note |
|---|---|---|
|SECONDS_PER_DAY|86400.0||
|MINUTES_PER_DAY|1440.0||
|DAYS_PER_JCENT|36525.0||
|JD_J2000|2451545.0||
|AU_KM, R_EARTH_KM, R_SUN_KM, F_EARTH|-|WGS-84 and IAU values|
|PI, TWO_PI, DEG2RAD, RAD2DEG|-|See AST0021|

## Design

### Diagrams

## Change Log

| Date | Description |
|---|---|
| TBD | Initial draft |
