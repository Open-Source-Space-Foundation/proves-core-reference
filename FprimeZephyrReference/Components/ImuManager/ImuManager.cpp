// ======================================================================
// \title  ImuManager.cpp
// \brief  cpp file for ImuManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ImuManager/ImuManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuManager ::ImuManager(const char* const compName) : ImuManagerComponentBase(compName) {}

ImuManager ::~ImuManager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------
void ImuManager ::configure(const struct device* lis2mdl, const struct device* lsm6dso) {
    this->m_lis2mdl = lis2mdl;
    this->m_lsm6dso = lsm6dso;

    struct sensor_value magn_odr = this->getMagnetometerSamplingFrequency();
    struct sensor_value accel_odr = this->getAccelerometerSamplingFrequency();
    struct sensor_value gyro_odr = this->getGyroscopeSamplingFrequency();
    this->configureSensors(magn_odr, accel_odr, gyro_odr);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Success condition;  // Ignoring for now
    Drv::Acceleration acceleration = this->accelerationGet_handler(0, condition);
    Drv::AngularVelocity angular_velocity = this->angularVelocityGet_handler(0, condition);
    Drv::MagneticField magnetic_field = this->magneticFieldGet_handler(0, condition);

    // Check if parameters have changed, and reconfigure sensors if they have
    struct sensor_value magn_odr = this->getMagnetometerSamplingFrequency();
    struct sensor_value accel_odr = this->getAccelerometerSamplingFrequency();
    struct sensor_value gyro_odr = this->getGyroscopeSamplingFrequency();
    if (!this->sensorValuesEqual(&magn_odr, &this->m_curr_magn_odr) ||
        !this->sensorValuesEqual(&accel_odr, &this->m_curr_accel_odr) ||
        !this->sensorValuesEqual(&gyro_odr, &this->m_curr_gyro_odr)) {
        this->configureSensors(magn_odr, accel_odr, gyro_odr);
    }
}

Drv::Acceleration ImuManager ::accelerationGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!device_is_ready(this->m_lsm6dso)) {
        this->log_WARNING_HI_Lsm6dsoDeviceNotReady();
        return Drv::Acceleration(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_Lsm6dsoDeviceNotReady_ThrottleClear();

    sensor_sample_fetch_chan(this->m_lsm6dso, SENSOR_CHAN_ACCEL_XYZ);

    struct sensor_value x, y, z;
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_ACCEL_Z, &z);

    this->applyAxisOrientation(x, y, z);

    Drv::Acceleration acceleration =
        Drv::Acceleration(sensor_value_to_double(&x), sensor_value_to_double(&y), sensor_value_to_double(&z));

    this->tlmWrite_Acceleration(acceleration);

    condition = Fw::Success::SUCCESS;
    return acceleration;
}

Drv::AngularVelocity ImuManager ::angularVelocityGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!device_is_ready(this->m_lsm6dso)) {
        this->log_WARNING_HI_Lsm6dsoDeviceNotReady();
        return Drv::AngularVelocity(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_Lsm6dsoDeviceNotReady_ThrottleClear();

    sensor_sample_fetch_chan(this->m_lsm6dso, SENSOR_CHAN_GYRO_XYZ);

    struct sensor_value x, y, z;
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(this->m_lsm6dso, SENSOR_CHAN_GYRO_Z, &z);

    this->applyAxisOrientation(x, y, z);

    Drv::AngularVelocity angular_velocity =
        Drv::AngularVelocity(sensor_value_to_double(&x), sensor_value_to_double(&y), sensor_value_to_double(&z));

    this->tlmWrite_AngularVelocity(angular_velocity);

    condition = Fw::Success::SUCCESS;
    return angular_velocity;
}

Drv::MagneticField ImuManager ::magneticFieldGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!device_is_ready(this->m_lis2mdl)) {
        this->log_WARNING_HI_Lis2mdlDeviceNotReady();
        return Drv::MagneticField(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_Lis2mdlDeviceNotReady_ThrottleClear();

    sensor_sample_fetch_chan(this->m_lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    struct sensor_value x, y, z;
    sensor_channel_get(this->m_lis2mdl, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(this->m_lis2mdl, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(this->m_lis2mdl, SENSOR_CHAN_MAGN_Z, &z);

    this->applyAxisOrientation(x, y, z);

    Drv::MagneticField magnetic_field =
        Drv::MagneticField(sensor_value_to_double(&x), sensor_value_to_double(&y), sensor_value_to_double(&z));

    this->tlmWrite_MagneticField(magnetic_field);

    condition = Fw::Success::SUCCESS;
    return magnetic_field;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

void ImuManager ::configureSensors(struct sensor_value& magn, struct sensor_value& accel, struct sensor_value& gyro) {
    // Configure the lis2mdl
    if (sensor_attr_set(this->m_lis2mdl, SENSOR_CHAN_MAGN_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &magn) != 0) {
        this->log_WARNING_HI_MagnetometerSamplingFrequencyNotConfigured();
    } else {
        this->m_curr_magn_odr = magn;
    }

    // Configure the lsm6dso
    if (sensor_attr_set(this->m_lsm6dso, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &accel) != 0) {
        this->log_WARNING_HI_AccelerometerSamplingFrequencyNotConfigured();
    } else {
        this->m_curr_accel_odr = accel;
    }
    if (sensor_attr_set(this->m_lsm6dso, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &gyro) != 0) {
        this->log_WARNING_HI_GyroscopeSamplingFrequencyNotConfigured();
    } else {
        this->m_curr_gyro_odr = gyro;
    }
}

void ImuManager ::applyAxisOrientation(struct sensor_value& x, struct sensor_value& y, struct sensor_value& z) {
    Fw::ParamValid valid;
    Components::AxisOrientation::T orientation = this->paramGet_AXIS_ORIENTATION(valid);

    this->tlmWrite_AxisOrientation(orientation);

    switch (orientation) {
        case Components::AxisOrientation::ROTATED_90_DEG_CW: {
            const struct sensor_value temp_x = x;
            x = y;
            sensor_value_from_double(&y, -sensor_value_to_double(&temp_x));
            return;
        }
        case Components::AxisOrientation::ROTATED_90_DEG_CCW: {
            const struct sensor_value temp_x = x;
            sensor_value_from_double(&x, -sensor_value_to_double(&y));
            y = temp_x;
            return;
        }
        case Components::AxisOrientation::ROTATED_180_DEG: {
            sensor_value_from_double(&x, -sensor_value_to_double(&x));
            sensor_value_from_double(&y, -sensor_value_to_double(&y));
            return;
        }
        case Components::AxisOrientation::STANDARD: {
            return;
        }
    }
}

struct sensor_value ImuManager ::getAccelerometerSamplingFrequency() {
    Fw::ParamValid valid;
    Components::Lsm6dsoSamplingFrequency freqParam = this->paramGet_ACCELEROMETER_SAMPLING_FREQUENCY(valid);

    this->tlmWrite_AccelerometerSamplingFrequency(freqParam);

    return this->getLsm6dsoSamplingFrequency(freqParam);
}

struct sensor_value ImuManager ::getGyroscopeSamplingFrequency() {
    Fw::ParamValid valid;
    Components::Lsm6dsoSamplingFrequency freqParam = this->paramGet_GYROSCOPE_SAMPLING_FREQUENCY(valid);

    this->tlmWrite_GyroscopeSamplingFrequency(freqParam);

    return this->getLsm6dsoSamplingFrequency(freqParam);
}

struct sensor_value ImuManager ::getLsm6dsoSamplingFrequency(Components::Lsm6dsoSamplingFrequency freqParam) {
    switch (freqParam) {
        case Components::Lsm6dsoSamplingFrequency::SF_12_5Hz:
            return sensor_value{12, 500000};
        case Components::Lsm6dsoSamplingFrequency::SF_26Hz:
            return sensor_value{26, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_52Hz:
            return sensor_value{52, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_104Hz:
            return sensor_value{104, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_208Hz:
            return sensor_value{208, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_416Hz:
            return sensor_value{416, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_833Hz:
            return sensor_value{833, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_1_66kHz:
            return sensor_value{1666, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_3_33kHz:
            return sensor_value{3333, 0};
        case Components::Lsm6dsoSamplingFrequency::SF_6_66kHz:
            return sensor_value{6666, 0};
    }

    FW_ASSERT(0, static_cast<I32>(freqParam));

    return sensor_value{0, 0};
}

struct sensor_value ImuManager::getMagnetometerSamplingFrequency() {
    Fw::ParamValid valid;
    Components::Lis2mdlSamplingFrequency freqParam = this->paramGet_MAGNETOMETER_SAMPLING_FREQUENCY(valid);

    this->tlmWrite_MagnetometerSamplingFrequency(freqParam);

    switch (freqParam) {
        case Components::Lis2mdlSamplingFrequency::SF_10Hz:
            return sensor_value{10, 0};
        case Components::Lis2mdlSamplingFrequency::SF_20Hz:
            return sensor_value{20, 0};
        case Components::Lis2mdlSamplingFrequency::SF_50Hz:
            return sensor_value{50, 0};
        case Components::Lis2mdlSamplingFrequency::SF_100Hz:
            return sensor_value{100, 0};
    }

    FW_ASSERT(0, static_cast<I32>(freqParam));

    return sensor_value{0, 0};
}

bool ImuManager ::sensorValuesEqual(struct sensor_value* sv1, struct sensor_value* sv2) {
    return (sv1->val1 == sv2->val1) && (sv1->val2 == sv2->val2);
}

}  // namespace Components
