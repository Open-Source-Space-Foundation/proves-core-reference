/**
 * \file WmmModel.cpp
 * \brief WMM2025 evaluation.
 */
#include "WmmModel.hpp"

// Constant used by XYZGeomag.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lib/XYZgeomag.hpp"

namespace Gnc {
namespace Environment {

FieldResult evaluateField(const FieldQuery& query, const FieldConfig& cfg, FieldSolution& out) {
    /*
     * ----------------------------------------------------------------------------
     * Step 1: Range gate.
     *
     * decYear is written before the gate so the attempted value can be seen in telemetry.
     * ----------------------------------------------------------------------------
     */
    out.decYear = static_cast<float>(Astro::decimalYearFromJd(query.jdUt1));

    const float altKmF = static_cast<float>(query.altKm);
    if (altKmF < cfg.minAltKm || altKmF > cfg.maxAltKm) {
        return FieldResult::ALTITUDE_OUT_OF_RANGE;
    }
    if (out.decYear < cfg.minYear || out.decYear > cfg.maxYear) {
        return FieldResult::EPOCH_OUT_OF_RANGE;
    }

    /*
     * ----------------------------------------------------------------------------
     * Step 2: TEME -> ECEF.
     *
     * A rotation about the 3-axis by GMST.
     * ----------------------------------------------------------------------------
     */
    const Astro::Vec3 ecefKm = Astro::temeToEcef(query.posTemeKm, query.gmstRad);

    /*
     * ----------------------------------------------------------------------------
     * Step 3: Evaluate.
     *
     * Altitude is converted from kilometers to meters(ECEF/ITRS) for XYZGeomag
     *
     * ----------------------------------------------------------------------------
     */
    geomag::Vector p;
    p.x = static_cast<float>(ecefKm.x * KM_TO_M);
    p.y = static_cast<float>(ecefKm.y * KM_TO_M);
    p.z = static_cast<float>(ecefKm.z * KM_TO_M);

    const geomag::Vector bEcefTesla = geomag::GeoMag(out.decYear, p, geomag::WMM2025);

    /*
     * ----------------------------------------------------------------------------
     * Step 4: ECEF -> TEME, Tesla -> nanotesla.
     *
     * Same GMST, opposite sign.
     * ----------------------------------------------------------------------------
     */
    const Astro::Vec3 bTesla{static_cast<double>(bEcefTesla.x), static_cast<double>(bEcefTesla.y),
                             static_cast<double>(bEcefTesla.z)};

    const Astro::Vec3 bTeme = Astro::rot3(bTesla, -query.gmstRad);

    out.fieldTemeNt = Astro::vscale(bTeme, TESLA_TO_NT);
    out.magnitudeNt = Astro::vnorm(out.fieldTemeNt);

    //
    if (!(out.magnitudeNt > cfg.minMagnitudeNt)) {
        return FieldResult::DEGENERATE_FIELD;
    }

    return FieldResult::OK;
}

}  // namespace Environment
}  // namespace Gnc
