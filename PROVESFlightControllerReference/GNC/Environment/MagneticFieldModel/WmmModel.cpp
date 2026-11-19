/**
 * \file WmmModel.cpp
 * \brief WMM evaluation using configurable coefficients.
 */

#include "WmmModel.hpp"
#include "Os/File.hpp"

#include <cmath>
#include <cstdlib>
#include <cctype>

// Constant used by XYZGeomag.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lib/XYZgeomag.hpp"

namespace Gnc {
namespace Environment {
FieldResult evaluateField(const FieldQuery& query, const FieldConfig& cfg, FieldSolution& out, const geomag::ConstModel& model) {
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

    const geomag::Vector bEcefTesla = geomag::GeoMag(out.decYear, p, model);

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


/**
 * Read one line from a WMM coefficient file.
 *
 * Returns false on a read error, an empty EOF,
 * or when the line exceeds the buffer capacity.
 */
static bool readWmmLine(Os::File& file, char* buffer, FwSizeType capacity) {
    if (buffer == nullptr || capacity < 2) {
        return false;
    }

    FwSizeType length = 0;

    while (length < capacity - 1) {
        U8 character = 0;
        FwSizeType bytesRead = 1;

        const Os::File::Status status =
            file.read(&character, bytesRead, Os::File::WaitType::WAIT);

        // An actual read error must not be treated as success.
        if (status != Os::File::OP_OK) {
            buffer[length] = '\0';
            return false;
        }

        // End of file: accept a final line without '\n'.
        if (bytesRead == 0) {
            buffer[length] = '\0';
            return length > 0;
        }

        if (character == '\n') {
            buffer[length] = '\0';
            return true;
        }

        buffer[length++] = static_cast<char>(character);
    }

    // Buffer filled before the line ended.
    buffer[length] = '\0';
    return false;
}



bool loadWmmCoefficients(const char* filePath, geomag::ConstModel& model) {
    if (filePath == nullptr || filePath[0] == '\0') {
        return false;
    }

    Os::File file;
    if (file.open(filePath, Os::File::OPEN_READ) != Os::File::OP_OK) {
        return false;
    }

    // Keep the original model unchanged until all data is validated.
    geomag::ConstModel loadedModel{};
    char line[160];

    // Read the header (example: 2025.0 WMM-2025 11/13/2024).
    if (!readWmmLine(file, line, sizeof(line))) {
        file.close();
        return false;
    }

    char* end = nullptr;
    loadedModel.epoch = std::strtof(line, &end);

    if (end == line || !std::isfinite(loadedModel.epoch) ||
        loadedModel.epoch < 1900.0f || loadedModel.epoch > 2200.0f) {
        file.close();
        return false;
    }

    int expectedN = 1;
    int expectedM = 0;

    // A degree-12 WMM has 90 coefficient records.
    for (int record = 0; record < geomag::NUMCOF - 1; ++record) {
        if (!readWmmLine(file, line, sizeof(line))) {
            file.close();
            return false;
        }

        char* cursor = line;
        char* next = nullptr;

        const long n = std::strtol(cursor, &next, 10);
        if (next == cursor) {
            file.close();
            return false;
        }
        cursor = next;

        const long m = std::strtol(cursor, &next, 10);
        if (next == cursor) {
            file.close();
            return false;
        }
        cursor = next;

        float values[4];

        for (int i = 0; i < 4; ++i) {
            values[i] = std::strtof(cursor, &next);

            if (next == cursor || !std::isfinite(values[i])) {
                file.close();
                return false;
            }
            cursor = next;
        }

        // Reject unexpected extra data on a coefficient line.
        while (*cursor != '\0' &&
               std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }

        if (*cursor != '\0' || n != expectedN || m != expectedM) {
            file.close();
            return false;
        }

        // Convert Schmidt-normalized coefficients to the
        // unnormalized representation expected by XYZgeomag.
        double factorSquared = (m == 0) ? 1.0 : 2.0;

        for (int k = static_cast<int>(n - m + 1);
             k <= static_cast<int>(n + m); ++k) {
            factorSquared /= static_cast<double>(k);
        }

        const float factor =
            static_cast<float>(std::sqrt(factorSquared));

        const int index =
            (expectedM * (2 * geomag::NMAX - expectedM + 1)) / 2
            + expectedN;

        loadedModel.Main_Field_Coeff_C[index] = values[0] * factor;
        loadedModel.Main_Field_Coeff_S[index] = values[1] * factor;
        loadedModel.Secular_Var_Coeff_C[index] = values[2] * factor;
        loadedModel.Secular_Var_Coeff_S[index] = values[3] * factor;

        ++expectedM;

        if (expectedM > expectedN) {
            ++expectedN;
            expectedM = 0;
        }
    }

    file.close();

    // All required records passed validation.
    model = loadedModel;
    return true;
}


}  // namespace Environment
}  // namespace Gnc