// ======================================================================
// \title  LightSensor.cpp
// \author robertpendergrast
// \brief  cpp file for LightSensor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LightSensor/LightSensor.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensor ::LightSensor(const char* const compName) : LightSensorComponentBase(compName) {}

LightSensor ::~LightSensor() {}

void LightSensor ::configure(const struct device* dev) {
    return;
}

void LightSensor ::ReadData() {  // const struct device *dev,
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
            this->log_WARNING_HI_LightSensorError(errMsg);
        }

        // Set the sensor attribute for div4
        sen.val1 = paramGet_DIV4(valid);

        ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &sen);
        if (ret) {
            Fw::LogStringArg errMsg("Failed to set div4 attribute");
            this->log_WARNING_HI_LightSensorError(errMsg);
        }

        // Set the sensor attribute for the gain
        sen.val1 = 0;
        ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &sen);
        if (ret) {
            Fw::LogStringArg errMsg("Failed to set gain attribute ret");
            this->log_WARNING_HI_LightSensorError(errMsg);
        }

        this->m_attributes_set = true;
    }

    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("Light sensor not ready after timeout"));
    }

    // Get the rate
    ret = sensor_sample_fetch(this->m_dev);
    if ((ret < 0) && (ret != -E2BIG)) {
        Fw::LogStringArg errMsg("sample update error");
        this->log_WARNING_HI_LightSensorError(errMsg);
        this->log_WARNING_HI_LightSensorErrorInt(ret);
    }

    // Get the light data
    sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_LIGHT, &light);

    // Get the raw Ambient Light Sensor data
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

void LightSensor ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Logic state;
    this->gpioRead_out(portNum, state);
    if (state == Fw::Logic::HIGH) {  // port call to the gpio driver, pass in a specific pin #
                                     // this->ReadData();
        this->tlmWrite_RawLightData(this->m_RawLightData);
        this->tlmWrite_IRLightData(this->m_IRLightData);
        this->tlmWrite_ALSLightData(this->m_ALSLightData);
    } else {
        if (this->m_device_init == true) {
            this->m_device_init = false;
        }
        if (state == Fw::Logic::LOW && this->m_attributes_set == true) {
            this->m_attributes_set = false;
        }
    }
}

void LightSensor::init_handler(FwIndexType portNum) {
    // TEMPORARILY DISABLED - Device tree nodes not configured
    this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("LightSensor temporarily disabled"));
    return;
    
    /* COMMENTED OUT FOR TESTING
    const struct device* mux = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
    const struct device* channel = DEVICE_DT_GET(DT_NODELABEL(top_i2c));
    const struct device* sensor = DEVICE_DT_GET(DT_NODELABEL(top_light_sens));

    k_sleep(K_MSEC(50));

    if (!mux || !channel || !sensor) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("Device DT_NODELABEL missing"));
        return;
    }

    int ret = device_init(mux);
    if (ret < 0) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("TCA9548A init failed"));
        return;
    }
    k_sleep(K_MSEC(30));

    ret = device_init(channel);
    if (ret < 0) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("Mux channel init failed"));
        return;
    }
    k_sleep(K_MSEC(30));

    ret = device_init(sensor);
    if (ret < 0) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("Light sensor init failed"));
        // Continue anyway - might still work
    }
    k_sleep(K_MSEC(50));

    if (!device_is_ready(sensor)) {
        this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("Light sensor not ready after timeout"));
    }

    this->m_dev = sensor;
    this->m_device_init = true;

    this->log_ACTIVITY_LO_LightSensorConfigured();

    uint8_t dummy;

    printk("Scanning I2C bus...\n");

    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        // Try to read from the address
        if (i2c_read(channel, &dummy, 1, 0x29) == 0) {
            printk("Device found at address 0x%02X\n", addr);
            char addrMsg[64];
            snprintf(addrMsg, sizeof(addrMsg), "Device found at address 0x%02X", addr);
            this->log_WARNING_HI_LightSensorError(Fw::LogStringArg(addrMsg));
        } else {
            this->log_WARNING_HI_LightSensorError(Fw::LogStringArg("No Device at address"));
        }
    }
    END COMMENTED OUT SECTION */
}

}  // namespace Components
