// ======================================================================
// \title  LightSensor.cpp
// \author robertpendergrast
// \brief  cpp file for LightSensor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LightSensor/LightSensor.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/veml6031.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensor ::LightSensor(const char* const compName) : LightSensorComponentBase(compName) {}

LightSensor ::~LightSensor() {}

void LightSensor ::configure(const struct device *dev ) {
    // Configures the light sensor
    this->m_dev = dev;
    this->m_configured = true;
}

void LightSensor ::read_sensor_data(){

    int ret;
	struct sensor_value light;
	struct sensor_value als_raw;
	struct sensor_value ir_raw;
	struct sensor_value sen;

	sen.val2 = 0;

    Fw::ParamValid valid;
    sen.val1 = this->paramGet_INTEGRATION_TIME(valid);

    // Set integration time
	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &sen);
	if (ret) {
		printf("Failed to set it attribute ret: %d\n", ret);
	}

    sen.val1 = this->paramGet_DIV4(valid);
    // Set detection threshold
	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &sen);
	if (ret) {
		printf("Failed to set div4 attribute ret: %d\n", ret);
	}

    sen.val1 = this->paramGet_GAIN(valid);
    // Set gain
	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &sen);
	if (ret) {
		printf("Failed to set gain attribute ret: %d\n", ret);
	}

    // Fetch a new sample
	ret = sensor_sample_fetch(this->m_dev);
	if ((ret < 0) && (ret != -E2BIG)) {
		printf("sample update error. ret: %d\n", ret);
	}

    // Get the sensor values and store them into member variables
	sensor_channel_get(this->m_dev, SENSOR_CHAN_LIGHT, &light);
	sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_VEML6031_ALS_RAW_COUNTS, &als_raw);
	sensor_channel_get(this->m_dev, (enum sensor_channel)SENSOR_CHAN_VEML6031_IR_RAW_COUNTS, &ir_raw);

    this->light = sensor_value_to_double(&light);
    this->als = sensor_value_to_double(&als_raw);
    this->ir = sensor_value_to_double(&ir_raw);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void LightSensor ::run_handler(FwIndexType portNum, U32 context) {
    // Polls the light sensor and processes the data
    this->read_sensor_data();

    // Write the data to telemetry
    this->tlmWrite_als(this->als);
    this->tlmWrite_ir(this->ir);
    this->tlmWrite_light(this->light);
}

}  // namespace Components
