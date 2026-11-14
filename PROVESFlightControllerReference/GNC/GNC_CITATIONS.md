# GNC Module Constants & Equations Citations

Sorted by directory, file, and order of appearance in a file.
---

## `Gnc/AstroLib/` - `AstroLib.hpp`, `AstroLib.cpp`

C++ astrodynamics library. Most constants in module are from here.

### Math / angle constants
`PI`, `TWO_PI`, `DEG2RAD`, `RAD2DEG`, `ARCSEC2RAD`

- Standard math constants and unit conversions

### Time-scale constants

- **`JD_J2000 = 2451545.0`** - Julian Date of the standard epoch
  J2000.0 (2000 Jan 1, 12:00 TT).
  See [USNO - Julian Dates](https://aa.usno.navy.mil/data/JulianDate) and
  Vallado, D.A., *Fundamentals of Astrodynamics and Applications*, 4th/5th edition,
  Microcosm Press - Table of epochs, Ch. 3.

- **`JD_UNIX_EPOCH = 2440587.5`** - Julian Date of the Unix epoch
  (1970-01-01T00:00:00 UTC). Standard JD ↔ Unix-time conversion constant.
  Source: [USNO Julian Date converter](https://aa.usno.navy.mil/data/JulianDate)

- **`SEC_PER_DAY = 86400.0`**, **`MIN_PER_DAY = 1440.0`** - SI-day
  definitions (86400 SI seconds = 1 mean solar day of civil time). Source:
  [BIPM - SI Brochure](https://www.bipm.org/en/publications/si-brochure), §2.3.1.

- **`DAYS_PER_JCENT = 36525.0`** - Definition of the Julian century
  (36525 days of 86400 SI seconds each). Source: Vallado (above), Ch. 3, and the
  [IERS Conventions (2010), IERS Technical Note 36](https://www.iers.org/IERS/EN/Publications/TechnicalNotes/tn36.html), Ch. 5

### Earth / Sun physical constants

- **`AU_KM = 149597870.7`** - Astronomical unit (149 597 870 700 m) in kilometers.
  Sources: [IAU Resolution B2 (2012)](https://syrte.obspm.fr/IAU_resolutions/IAUResol_2012_0.html);
  [Wikipedia - Astronomical unit](https://en.wikipedia.org/wiki/Astronomical_unit).

- **`R_EARTH_KM = 6378.137`** and **`F_EARTH = 1/298.257223563`** -
  WGS-84 ellipsoid semi-major axis and flattening, the U.S. DoD/NGA
  reference ellipsoid.
  Source: National Geospatial-Intelligence Agency, *NIMA/NGA TR8350.2,
  "Department of Defense World Geodetic System 1984, Its Definition and
  Relationships with Local Geodetic Systems,"* 3rd ed. Defining
  parameters table (a = 6378137.0 m, 1/f = 298.257223563) Reproduced at:
  [UNOOSA - WGS 84 reference sheet](https://www.unoosa.org/pdf/icg/2012/template/WGS_84.pdf).

- **`R_SUN_KM = 696000.0`** - Nominal solar radius. Source: Vallado, *Fundamentals of
  Astrodynamics and Applications*;
  [IAU 2015 Resolution B3 nominal solar radius](https://www.iau.org/static/resolutions/IAU2015_English.pdf).

- **`TT_MINUS_TAI_SEC = 32.184`** - Defined offset between
  Terrestrial Time and International Atomic Time.
  Source: [IERS Conventions (2010), IERS Technical Note 36](https://www.iers.org/IERS/EN/Publications/TechnicalNotes/tn36.html), Ch. 1, eq. 1.1

### Earth shadow cone half-angles

- **`ALPHA_UMB_RAD = 0.264121°`**, **`ALPHA_PEN_RAD = 0.269007°`** -
  Umbral/penumbral shadow cone half-angles used in the dual-cone
  eclipse (shadow) algorithm.
  Source: Vallado, *Fundamentals of Astrodynamics and Applications*,
  **Algorithm 34 ("Shadow")**, Ch. 5 (Ephemeris/eclipse chapter).

### Greenwich Mean Sidereal Time - `gmst1982Rad()`

The polynomial
`67310.54841 + fmod(3155760000·T, 86400) + 8640184.812866·T + 0.093104·T² − 6.2e-6·T³`
is the standard **IAU 1982 GMST expression** (Aoki et al., 1982),
reformulated by Vallado to avoid loss of precision.

- Original polynomial form and coefficients: Aoki, S., Guinot, B.,
  Kaplan, G.H., Kinoshita, H., McCarthy, D.D., Seidelmann, P.K.,
  *"The new definition of universal time,"* Astronomy & Astrophysics,
  105, 359–361, 1982.
- Standard algorithmic form (`24110.54841 + 8640184.812866·T + ...`,
  from which the `67310.54841` split-precision form here is derived):
  Vallado, *Fundamentals of Astrodynamics and Applications*,
  **Algorithm 15 ("GSTIME")**.

### IAU-1980 nutation (4 dominant terms) - `nutation1980()`

The truncated series (`dPsi`, `dEps` from `sin(Ω)`, `sin(2L☉)`,
`sin(2L☾)`, `sin(2Ω)` with coefficients −17.20″, −1.32″, −0.23″,
0.21″ / 9.20″, 0.57″, 0.10″, −0.09″) is a 4-term truncation of the
IAU 1980 Theory of Nutation (106-term series).

- Full series and coefficients: Seidelmann, P.K. (ed.), *"1980 IAU
  Theory of Nutation: The Final Report of the IAU Working Group on
  Nutation,"* Celestial Mechanics, 27, 79–106, 1981; reproduced in the
  *Explanatory Supplement to the Astronomical Almanac* (1992), §3.222.
- Mean obliquity polynomial `23.439291111 − 0.0130041667·T − 1.64e-7·T²
  + 5.04e-7·T³` (degrees): IAU 1980 obliquity expression, also given in
  Vallado, *Fundamentals of Astrodynamics and Applications*, Ch. 3, and
  in the *Astronomical Almanac* (annual editions), §B.
- Equation-of-the-equinoxes kinematic-correction terms (0.00264″,
  0.000063″): the 1997 IAU corrections for the equation of the
  equinoxes, documented in the [IERS Conventions (2010), IERS Technical
  Note 36](https://www.iers.org/IERS/EN/Publications/TechnicalNotes/tn36.html).

### Frame rotations (`rot1`, `rot3`) and MOD→TOD→TEME transformation

Euler-axis coordinate-frame rotation matrices and their use in the precession/nutation transformation.
Source: Vallado, *Fundamentals of Astrodynamics and Applications*,
Ch. 3 ("Coordinate and Time Systems"), definitions of `ROT1`/`ROT3` and
the MOD→TOD→TEME/PEF transformation sequence (Vallado's Fig. 3-13 and
associated algorithms).

### WGS-84 geodetic conversion (Bowring's method) - `ecefToGeodetic()`

ECEF→geodetic-latitude conversion

- Original method: Bowring, B.R., *"Transformation from Spatial to
  Geographical Coordinates,"* Survey Review, 23(181), 323–327, 1976.
- Secondary description of the same formula:
  [GNU Gama manual - "Transformation from spatial to geographic
  coordinates"](https://www.gnu.org/software/gama/manual/html_node/Transformation-from-spatial-to-geographical-coordinates.html)

- Study displaying the accuracy of this conversion:
  Comparative study, Virginia Tech TR (1990), *"Comparing the accuracy
  and efficiency of algorithms for converting Cartesian to geodetic
  coordinates,"* found Bowring's 1976 procedure most accurate among 14
  tested methods: [VTechWorks record](https://vtechworks.lib.vt.edu/items/5ea9af75-6b05-4901-99f6-d982f6388c49).

### Low-precision solar ephemeris - `sunLowPrecisionMod()`

Mean longitude `280.460 + 36000.771·T`, mean anomaly
`357.5291092 + 35999.05034·T`, equation-of-center coefficients
`1.914666471`, `0.019994643`, radius-vector series
`1.000140612 − 0.016708617·cos(M) − 0.000139589·cos(2M)`, and mean
obliquity `23.439291 − 0.0130042·T` (all degrees).

- Primary source: Vallado,
  *Fundamentals of Astrodynamics and Applications*, **Algorithm 29
  ("Sun")**, based on the *Astronomical Almanac*'s low-precision solar
  coordinate formulas.
- Original Almanac formula (same form, slightly
  different-precision coefficients across editions): *The Astronomical
  Almanac*, section C ("Low precision formulas for the Sun"), reproduced here:
  [USNO - "Approximate Solar Coordinates"](https://aa.usno.navy.mil/faq/sun_approx).


### Dual-cone eclipse / shadow geometry - `shadowConical()`

Distributed form of the umbra/penumbra vertical
extent equations. Source: Vallado, *Fundamentals of Astrodynamics and
Applications*, **Algorithm 34 ("Shadow")**.

### Cylindrical shadow model - `isSunlitCylindrical()`

Simplified shadow test comparing perpendicular offset from the Sun line
to `R_EARTH_KM²`; A commonly-used simplification of the
dual-cone model. See Vallado (above), remarks preceding Algorithm 34.

### Beta angle - `betaAngleRad()`

`beta = 90° − angle(h, sunUnit)`, computed with `atan2` for numerical
conditioning near ±90°.
Source: Vallado, *Fundamentals of Astrodynamics and Applications*, Ch.
5 (eclipse/beta-angle discussion).

---

## `Gnc/Adcs/AttitudeDetermination/` - `TriadSolver.hpp`, `TriadSolver.cpp`, `AttitudeDetermination.cpp/.hpp`, `TriadTypes.fpp`

### TRIAD algorithm

Tri-axial attitude determination method:
(`t1 = v1`, `t2 = (v1×v2)/|v1×v2|`, `t3 = t1×t2`, `A = M_body · M_ref^T`).

- Original publication: Black, H.D., *"A Passive System for Determining
  the Attitude of a Satellite,"* AIAA Journal, 2(7), 1350–1351, July
  1964. See [Wikipedia - TRIAD
  method](https://en.wikipedia.org/wiki/Triad_method) and its references.

- Textbook and paper with the same matrix
  formulation (`A = M_o M_r^T`) used here: *Spacecraft Attitude Determination and Control*, Springer, 1978,
  pp. 420–428; [arXiv:1609.07436 - "UAV attitude estimation using Unscented
  Kalman Filter and TRIAD,"](https://arxiv.org/pdf/1609.07436) §C ("The TRIAD algorithm").


### DCM→quaternion conversion (Shepperd's method)

Used by Eigen's `Quaternionf(Matrix3f)`.

- Original method: Shepperd, S.W., *"Quaternion from Rotation
  Matrix,"* Journal of Guidance and Control, 1(3), 223–224, 1978.

---

## `Gnc/Environment/MagneticFieldModel/` - `MagneticFieldModel.cpp/.hpp`, `lib/XYZgeomag.hpp`

### World Magnetic Model 2025 (WMM2025)

The Gauss coefficients in `XYZgeomag.hpp` and the
validated ranges `WMM_MIN_YEAR/MAX_YEAR = 2025.0–2030.0`,
`WMM_MIN_ALT_KM/MAX_ALT_KM = −1–850 km`.

- Official model and technical report: NOAA National Centers for
  Environmental Information (NCEI) & British Geological Survey (BGS),
  *"The US/UK World Magnetic Model for 2025–2030: Technical Report,"*
  2024. Landing page / DOI: [NOAA NCEI - World Magnetic Model
  2025](https://www.ncei.noaa.gov/products/world-magnetic-model) (dataset DOI:
  10.25921/aqfd-sd83, see [metadata record](https://www.ncei.noaa.gov/metadata/geoportal/rest/metadata/item/gov.noaa.ngdc:WMM2025/html)).
  Validity period (2025.0–2030.0) confirmed on the same page.

- Plain-language summary:
  [NOAA NCEI - "World Magnetic Model Receives Upgrade"](https://www.ncei.noaa.gov/news/world-magnetic-model-receives-upgrade).

### Mean Earth radius used in the spherical-harmonic recursion

`EARTH_R = 6371200.0` m - The reference sphere radius specified for
evaluating the WMM's spherical harmonic series.
Source: cited in `XYZgeomag.hpp` as "mean radius of ellipsoid
in meters from section 1.2 of the WMM2015 Technical Report" The 2025 report (above)
uses the same reference radius.

### Spherical harmonic synthesis algorithm

The recursive Legendre/associated-Legendre evaluation (`Vnm`, `Wnm`
recursion) used to sum the Gauss coefficients into a field vector.
Source: Cited in in `XYZgeomag.hpp` as using
"a different spherical harmonics calculation, described in sections
3.2.4 and 3.2.5" of Montenbruck, O. and Gill, E., *Satellite Orbits:
Models, Methods, and Applications*, Springer, 2000.

### Library attribution

`XYZgeomag.hpp` - header-only WMM evaluation library.
Source: Zimmerberg, N. (Cornell University), *XYZgeomag*, MIT License,
2019–2021; repository: [github.com/nathanzimmerberg/XYZgeomag](https://github.com/nathanzimmerberg/XYZgeomag).

### Unit conversions

`TESLA_TO_NT = 1e9`, `KM_TO_M = 1000.0` - SI unit definitions.
Source: [BIPM - SI Brochure](https://www.bipm.org/en/publications/si-brochure), §3 (SI prefixes).

---

## `Gnc/Environment/SolarEphemeris/` - `SolarEphemeris.cpp/.hpp`

No new physical constants are introduced in this file
---

## `Gnc/Environment/OrbitPropagator/` - `OrbitPropagator.cpp/.hpp/.fpp`

### SGP4 propagator

Propagation is handled by the third-party `perturb` library, which wraps Vallado's reference
SGP4 implementation.

- `perturb` library: Ranu, G., *perturb* (C++11 SGP4 wrapper),
  repository: [github.com/gunvirranu/perturb](https://github.com/gunvirranu/perturb)

- Underlying SGP4 theory and reference source code: Vallado,
  D.A., Crawford, P., Hujsak, R., Kelso, T.S., *"Revisiting Spacetrack
  Report #3,"* AIAA/AAS Astrodynamics Specialist Conference, 2006
  (AIAA 2006-6753). Reference implementation mirror:
  [CelesTrak - Vallado SGP4 source](https://celestrak.org/software/vallado-sw.php).

### Time-scale / leap-second parameters (`OrbitPropagator.fpp`)

- **`LEAP_SEC` default `37.0`** - TAI−UTC leap-second count. This is
  a time-varying quantity set by international agreement, so it must be updated whenever IERS announces
  a new leap second. Current value and history:
  [IERS - Bulletin C (leap second announcements)](https://datacenter.iers.org/data/latestVersion/bulletinC.txt).

- **`DUT1_SEC` default `0.0`** - By IERS convention, leap seconds are inserted to keep |UT1−UTC| < 0.9 s.
  Source: [IERS - Bulletin A](https://datacenter.iers.org/data/latestVersion/bulletinA.txt) and
  [IERS Conventions (2010), IERS Technical Note 36](https://www.iers.org/IERS/EN/Publications/TechnicalNotes/tn36.html), Ch. 5
  (definition of UT1 and the 0.9 s leap-second insertion criterion).

- **`MAX_TLE_AGE_DAYS` default `7.0`** - SGP4 accuracy degrades roughly 1–3 km per day past TLE epoch.
  See Vallado, D.A. & Crawford, P., *"SGP4 Orbit Determination,"*
  AIAA/AAS Astrodynamics Specialist Conference, 2008 (AIAA 2008-6770),
  and the TLE-accuracy discussion at
  [CelesTrak - FAQs, "How current do the elements need to be?"](https://celestrak.org/columns/v04n12/).

### TLE line length (69 characters)

Standard NORAD Two-Line Element Set fixed-column format.
Source: [CelesTrak - TLE format description](https://celestrak.org/NORAD/documentation/tle-fmt.php)

---

## Summary

| Constant / Algorithm | File(s) | Primary citation |
|---|---|---|
| Astronomical unit (149597870.7 km) | `AstroLib.hpp` | IAU 2012 Resolution B2 |
| WGS-84 ellipsoid (a, f) | `AstroLib.hpp`, `XYZgeomag.hpp` | NGA TR8350.2 |
| TT − TAI = 32.184 s | `AstroLib.hpp` | IERS Conventions (2010), TN36 |
| Earth shadow cone half-angles | `AstroLib.hpp/.cpp` | Vallado, Alg. 34 |
| GMST (IAU 1982) | `AstroLib.cpp` | Aoki et al. 1982; Vallado Alg. 15 |
| Nutation (IAU 1980, 4 terms) | `AstroLib.cpp` | Seidelmann 1981 (IAU WG report) |
| Bowring ECEF→geodetic | `AstroLib.cpp` | Bowring 1976, Survey Review |
| Low-precision solar ephemeris | `AstroLib.cpp` | Vallado Alg. 29 / Astronomical Almanac |
| TRIAD algorithm | `TriadSolver.hpp/.cpp` | Black 1964, AIAA Journal |
| Shepperd DCM→quaternion | `TriadSolver.cpp` (via Eigen) | Shepperd 1978, J. Guid. Control |
| WMM2025 coefficients | `WMM2025.COF`, `XYZgeomag.hpp` | NOAA NCEI / BGS Technical Report |
| Spherical-harmonic recursion | `XYZgeomag.hpp` | Montenbruck & Gill 2000, §3.2 |
| SGP4 propagator | `OrbitPropagator.cpp/.hpp` | Vallado, Crawford, Hujsak, Kelso 2006 |
| Leap seconds / DUT1 | `OrbitPropagator.fpp` | IERS Bulletins A & C |
| TLE format | `OrbitPropagator.fpp` | CelesTrak TLE format spec |

---
