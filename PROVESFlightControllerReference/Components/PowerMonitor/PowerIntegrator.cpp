// ======================================================================
// \title  PowerIntegrator.cpp
// \brief  cpp file for PowerIntegrator class
// ======================================================================

#include "PROVESFlightControllerReference/Components/PowerMonitor/PowerIntegrator.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PowerIntegrator ::PowerIntegrator()
    : m_consumption_mWh(0.0), m_generation_mWh(0.0), m_lastUpdateTime_s(0.0), m_initialized(false) {}

PowerIntegrator ::~PowerIntegrator() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void PowerIntegrator ::update(double now_s, double consumedW, double generatedW) {
    // Initialize time on first call
    if (!this->m_initialized) {
        this->m_lastUpdateTime_s = now_s;
        this->m_initialized = true;
        return;
    }

    // Measure the time step once so both totals are integrated over the same interval
    double dt_s = now_s - this->m_lastUpdateTime_s;
    this->m_lastUpdateTime_s = now_s;

    accumulate(this->m_consumption_mWh, consumedW, dt_s);
    accumulate(this->m_generation_mWh, generatedW, dt_s);
}

void PowerIntegrator ::resetConsumption() {
    this->m_consumption_mWh = 0.0;
}

void PowerIntegrator ::resetGeneration() {
    this->m_generation_mWh = 0.0;
}

float PowerIntegrator ::getConsumption_mWh() const {
    return static_cast<float>(this->m_consumption_mWh);
}

float PowerIntegrator ::getGeneration_mWh() const {
    return static_cast<float>(this->m_generation_mWh);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void PowerIntegrator ::accumulate(double& total_mWh, double powerW, double dt_s) {
    // Guard against invalid power values
    if (powerW < 0.0 || powerW > 1000.0) {  // Sanity check: power should be 0-1000W
        return;
    }

    // Only accumulate if time has passed and delta is reasonable (< 10 seconds to avoid time jumps)
    if (dt_s > 0.0 && dt_s < 10.0) {
        // Convert to mWh: Power (W) * time (hours) * 1000
        total_mWh += powerW * (dt_s / 3600.0) * 1000.0;
    }
}

}  // namespace Components
