// ======================================================================
// \title  Velm6031Manager.cpp
// \brief  cpp file for Velm6031Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Velm6031Manager/Velm6031Manager.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Velm6031Manager ::Velm6031Manager(const char* const compName) : Velm6031ManagerComponentBase(compName) {}

Velm6031Manager ::~Velm6031Manager() {}

void Velm6031Manager ::configure(const struct device* dev) {
    this->m_dev = dev;
}

void Velm6031Manager ::ReadData() {  // const struct device *dev,
    // Return integer
    int ret;

    // Sensor value structs for reading data
    struct sensor_value light;
    struct sensor_value als_raw;
    struct sensor_value ir_raw;
    struct sensor_value sen;

    sen.val2 = 0;

    Fw::ParamValid valid;
    sen.val1 = 5;  // pass in saying that the parameter is valid

    // Setting the integration time attribute for the light sensor

    if (!(this->m_attributes_set)) {
        ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &sen);
        if (ret) {
            Fw::LogStringArg errMsg("Failed to set it attribute");
            // this->log_WARNING_HI_Velm6031ManagerError(errMsg);
        }

        // Set the sensor attribute for div4
        // sen.val1 = paramGet_DIV4(valid);

        ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &sen);
        if (ret) {
            Fw::LogStringArg errMsg("Failed to set div4 attribute");
            // this->log_WARNING_HI_Velm6031ManagerError(errMsg);
        }

        // Set the sensor attribute for the gain
        sen.val1 = 0;
        ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &sen);
        if (ret) {
            Fw::LogStringArg errMsg("Failed to set gain attribute ret");
            // this->log_WARNING_HI_Velm6031ManagerError(errMsg);
        }

        this->m_attributes_set = true;
    }

    // Get the rate
    ret = sensor_sample_fetch(this->m_dev);
    if ((ret < 0) && (ret != -E2BIG)) {
        Fw::LogStringArg errMsg("sample update error");
        // this->log_WARNING_HI_Velm6031ManagerError(errMsg);
        // this->log_WARNING_HI_Velm6031ManagerErrorInt(ret);
    }

    // Get the light data
    sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_LIGHT, &light);

    // Get the raw ALS
    sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_VEML6031_ALS_RAW_COUNTS, &als_raw);

    // Get the raw IR
    sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_VEML6031_IR_RAW_COUNTS, &ir_raw);

    this->m_RawLightData = sensor_value_to_double(&light);
    this->m_ALSLightData = sensor_value_to_double(&als_raw);
    this->m_IRLightData = sensor_value_to_double(&ir_raw);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Velm6031Manager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Logic state;
    // this->gpioRead_out(portNum, state);
    if (state == Fw::Logic::HIGH) {  // port call to the gpio driver, pass in a specific pin #
        this->ReadData();
        // this->tlmWrite_RawLightData(this->m_RawLightData);
        // this->tlmWrite_IRLightData(this->m_IRLightData);
        // this->tlmWrite_ALSLightData(this->m_ALSLightData);
    } else {
        if (this->m_device_init == true) {
            this->m_device_init = false;
        }
        if (state == Fw::Logic::LOW && this->m_attributes_set == true) {
            this->m_attributes_set = false;
        }
    }
}

void Velm6031Manager::init_handler(FwIndexType portNum) {
    // const struct device* mux = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
    // const struct device* channel = DEVICE_DT_GET(DT_NODELABEL(face0_i2c));
    // const struct device* sensor = DEVICE_DT_GET(DT_NODELABEL(face0_light_sens));

    // if (!mux || !channel || !sensor) {
    //     // this->log_WARNING_HI_Velm6031ManagerError(Fw::LogStringArg("Device DT_NODELABEL missing"));
    //     return;
    // }

    // int ret = device_init(mux);
    // if (ret < 0) {
    //     // this->log_WARNING_HI_Velm6031ManagerError(Fw::LogStringArg("TCA9548A init failed"));
    //     return;
    // }

    // ret = device_init(channel);
    // if (ret < 0) {
    //     // this->log_WARNING_HI_Velm6031ManagerError(Fw::LogStringArg("Mux channel init failed"));
    //     return;
    // }

    // ret = device_init(sensor);
    // if (ret < 0) {
    //     // this->log_WARNING_HI_Velm6031ManagerError(Fw::LogStringArg("Light sensor init failed"));
    //     // Continue anyway - might still work
    // }

    // if (!device_is_ready(sensor)) {
    //     // this->log_WARNING_HI_Velm6031ManagerError(Fw::LogStringArg("Light sensor not ready after timeout"));
    // }

    // this->m_dev = sensor;
    // this->m_device_init = true;

    // this->log_ACTIVITY_LO_Velm6031ManagerConfigured();
}

}  // namespace Drv
