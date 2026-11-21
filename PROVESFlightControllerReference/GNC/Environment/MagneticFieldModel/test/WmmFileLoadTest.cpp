
#include "../WmmModel.hpp"
#include "../lib/XYZgeomag.hpp"
#include "Os/Os.hpp"

#include <cmath>
#include <cstdio>

namespace {

bool compareArray(const float* actual, const float* expected,
                  int count, const char* name) {
    constexpr float TOLERANCE = 1.0e-3f;

    for (int i = 0; i < count; ++i) {
        if (std::fabs(actual[i] - expected[i]) > TOLERANCE) {
            std::printf("%s mismatch at index %d: %.8f vs %.8f\n",
                        name, i, actual[i], expected[i]);
            return false;
        }
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    Os::init();

    if (argc != 2) {
        std::printf("Usage: WmmFileLoadTest <path-to-WMM2025.COF>\n");
        return 1;
    }

    geomag::ConstModel loaded = geomag::WMM2025;

    if (!Gnc::Environment::loadWmmCoefficients(argv[1], loaded)) {
        const geomag::ConstModel& original = geomag::WMM2025;

        bool preserved = (loaded.epoch == original.epoch);

        preserved &= compareArray(
            loaded.Main_Field_Coeff_C,
            original.Main_Field_Coeff_C,
            geomag::NUMCOF,
            "Preserved Main C"
        );

        preserved &= compareArray(
            loaded.Main_Field_Coeff_S,
            original.Main_Field_Coeff_S,
            geomag::NUMCOF,
            "Preserved Main S"
        );

        preserved &= compareArray(
            loaded.Secular_Var_Coeff_C,
            original.Secular_Var_Coeff_C,
            geomag::NUMCOF,
            "Preserved Secular C"
        );

        preserved &= compareArray(
            loaded.Secular_Var_Coeff_S,
            original.Secular_Var_Coeff_S,
            geomag::NUMCOF,
            "Preserved Secular S"
        );

        if (!preserved) {
            std::printf("FAIL: Original WMM model was modified\n");
            return 1;
        }

        std::printf("PASS: Failed load preserved the original WMM model\n");
        return 0;
    }

    const geomag::ConstModel& expected = geomag::WMM2025;
    constexpr int COUNT = geomag::NUMCOF;

    bool passed = true;

    passed &= std::fabs(loaded.epoch - expected.epoch) < 1.0e-3f;

    passed &= compareArray(loaded.Main_Field_Coeff_C,
                           expected.Main_Field_Coeff_C,
                           COUNT, "Main C");

    passed &= compareArray(loaded.Main_Field_Coeff_S,
                           expected.Main_Field_Coeff_S,
                           COUNT, "Main S");

    passed &= compareArray(loaded.Secular_Var_Coeff_C,
                           expected.Secular_Var_Coeff_C,
                           COUNT, "Secular C");

    passed &= compareArray(loaded.Secular_Var_Coeff_S,
                           expected.Secular_Var_Coeff_S,
                           COUNT, "Secular S");

    if (!passed) {
        std::printf("FAIL: Loaded coefficients do not match WMM2025\n");
        return 1;
    }

    std::printf("PASS: All WMM2025 coefficients match\n");
    return 0;
}
