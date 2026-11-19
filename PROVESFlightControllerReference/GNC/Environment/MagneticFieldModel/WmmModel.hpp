/**
 * \file WmmModel.hpp
 * \brief WMM2025 field evaluation: kilometers(TEME) in, nanotesla(TEME) out.
 *
 * \details Evaluates WMM evaluation with the XYZgeomag library.
 *
 * Steps:
 *   1. Gate on the model's validated altitude and epoch range
 *   2. Rotate TEME -> ECEF(ITRS) by GMST
 *   3. Call the geomag library in ITRS meters, get back tesla
 *   4. Rotate ECEF -> TEME by the same GMST, scale to nanotesla
 */
#ifndef Gnc_Environment_WmmModel_HPP
#define Gnc_Environment_WmmModel_HPP

#include <cstdint>

#include "PROVESFlightControllerReference/GNC/AstroLib/AstroLib.hpp"
#include "lib/XYZgeomag.hpp"

namespace Gnc {
namespace Environment {

/*
 * ============================================================================
 * Constants
 * ============================================================================
 */

//! Tesla -> nanotesla
constexpr double TESLA_TO_NT = 1.0e9;

//! Kilometers -> meters.
constexpr double KM_TO_M = 1000.0;

/**
 * \brief Why an evaluation succeeded or failed.
 *
 * \details Both out-of-range codes trigger an EvaluationOutOfRange event.
 * This tells you which gate blocked the evaluation.
 */
enum class FieldResult : std::uint8_t {
    OK = 0,                     //!< Field computed and written to the output
    ALTITUDE_OUT_OF_RANGE = 1,  //!< Geodetic altitude outside of the valid range
    EPOCH_OUT_OF_RANGE = 2,     //!< Decimal year outside of the valid range
    DEGENERATE_FIELD = 3        //!< Field magnitude below configured minimum.
};

/**
 * Everything one evaluation needs, from Gnc::OrbitState.
 */
struct FieldQuery {
    Astro::Vec3 posTemeKm{0.0, 0.0, 0.0};  //!< Position, TEME, kilometers
    double altKm = 0.0;                    //!< Geodetic altitude above the WGS-84 ellipsoid, km
    double jdUt1 = 0.0;                    //!< UT1 Julian date, flattened
    double gmstRad = 0.0;                  //!< GMST at jdUt1, radians
};

/**
 * \brief Validity gates and the degeneracy threshold.
 *
 * \details Defaults are the WMM2025 validated ranges.
 *  Configurable for testing out-of-range gates.
 */
struct FieldConfig {
    /*
     * WMM2025 validated epoch range.
     */
    float minYear = 2025.0f;  //!< Lower epoch bound, decimal year
    float maxYear = 2030.0f;  //!< Upper epoch bound, decimal year

    /*
     * WMM validated altitude range, km above the WGS-84 ellipsoid.
     */
    float minAltKm = -1.0f;   //!< Lower altitude bound, km
    float maxAltKm = 850.0f;  //!< Upper altitude bound, km

    /**
     * Minimum magnitude for the unit vector to be meaningful. 1 nT is
     * much smaller than any real geomagnetic field, so this only trips
     * on a bad result.
     */
    double minMagnitudeNt = 1.0;
};

/**
 * \brief The field plus the quantities derived from it.
 *
 * \details decYear is always populated so out-of-range values can be seen in telemetry.
 */
struct FieldSolution {
    //! Field vector, TEME, nanotesla. Written only when OK is returned.
    Astro::Vec3 fieldTemeNt{0.0, 0.0, 0.0};

    //! |fieldTemeNt|, nanotesla
    double magnitudeNt = 0.0;

    /**
     * Decimal year the model was evaluated at.
     */
    float decYear = 0.0f;
};

/**
 * \brief Evaluate WMM2025 at one position and epoch.
 *
 * \param query  Position, altitude, epoch and GMST for this evaluation
 *
 * \param cfg    Validity gates and the degeneracy threshold
 *
 * \param out    [out] Field and diagnostics. fieldTemeNt, unitTeme,
 *               magnitudeNt and directionUsable are written only when
 *               OK is returned; decYear is always written.
 * \param model  WMM coefficients used for evaluation       
 *
 * \return FieldResult::OK on success, otherwise the gate that rejected
 *         the query
 */
FieldResult evaluateField(const FieldQuery& query,
                          const FieldConfig& cfg,
                          FieldSolution& out,
                          const geomag::ConstModel& model);

                          /**
 * Load WMM coefficients from a .COF file.
 *
 * Returns true if the file was loaded and validated successfully.
 * The existing model remains unchanged if loading fails.
 */
bool loadWmmCoefficients(const char* filePath, geomag::ConstModel& model);

}  // namespace Environment
}  // namespace Gnc

#endif  // Gnc_Environment_WmmModel_HPP
