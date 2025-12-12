// ======================================================================
// \title  Lis2mdlManager.cpp
// \brief  cpp file for Lis2mdlManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Lis2mdlManager/Lis2mdlManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lis2mdlManager ::Lis2mdlManager(const char* const compName) : Lis2mdlManagerComponentBase(compName) {}

Lis2mdlManager ::~Lis2mdlManager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void Lis2mdlManager ::configure(const struct device* dev) {
    this->m_dev = dev;

    struct sensor_value odr = this->getSamplingFrequency();

    if (sensor_attr_set(this->m_dev, SENSOR_CHAN_MAGN_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) != 0) {
        this->log_WARNING_HI_MagnetometerSamplingFrequencyNotConfigured();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::MagneticField Lis2mdlManager ::magneticFieldGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::MagneticField(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_MAGN_XYZ);

    sensor_channel_get(this->m_dev, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(this->m_dev, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(this->m_dev, SENSOR_CHAN_MAGN_Z, &z);

    Drv::MagneticField magnetic_readings =
        Drv::MagneticField(sensor_value_to_double(&x), sensor_value_to_double(&y), sensor_value_to_double(&z));

    this->tlmWrite_MagneticField(magnetic_readings);

    condition = Fw::Success::SUCCESS;
    return magnetic_readings;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

struct sensor_value Lis2mdlManager ::getSamplingFrequency() {
    Fw::ParamValid valid;
    Drv::Lis2mdlSamplingFrequency freqParam = this->paramGet_SAMPLING_FREQUENCY(valid);

    this->tlmWrite_SamplingFrequency(freqParam);

    switch (freqParam) {
        case Drv::Lis2mdlSamplingFrequency::SF_10Hz:
            return sensor_value{10, 0};
        case Drv::Lis2mdlSamplingFrequency::SF_20Hz:
            return sensor_value{20, 0};
        case Drv::Lis2mdlSamplingFrequency::SF_50Hz:
            return sensor_value{50, 0};
        case Drv::Lis2mdlSamplingFrequency::SF_100Hz:
            return sensor_value{100, 0};
    }

    FW_ASSERT(0, static_cast<I32>(freqParam));

    return sensor_value{0, 0};
}

}  // namespace Drv
