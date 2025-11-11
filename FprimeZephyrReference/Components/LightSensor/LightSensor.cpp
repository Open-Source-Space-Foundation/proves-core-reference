// ======================================================================
// \title  LightSensor.cpp
// \author robertpendergrast
// \brief  cpp file for LightSensor component implementation class
// ======================================================================



#include "FprimeZephyrReference/Components/LightSensor/LightSensor.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensor ::LightSensor(const char* const compName) : LightSensorComponentBase(compName) {}

LightSensor ::~LightSensor() {}

void LightSensor ::configure(const struct device* dev) {
    this->m_dev = dev;
    if (this->m_dev == nullptr) {
        this->log_WARNING_HI_LightSensorError();
        return;
    }

    this->m_configured = true;
}

void LightSensor ::ReadData() { // const struct device *dev, 
    // Return integer
    int ret;

    // Sensor value structs for reading data
	struct sensor_value light;
	struct sensor_value als_raw;
	struct sensor_value ir_raw;
	struct sensor_value sen;

	sen.val2 = 0;

    Fw::ParamValid valid;
	sen.val1 = paramGet_INTEGRATION_TIME(valid); // pass in saying that the parameter is valid

    // Setting the integration time attribute for the light sensor
	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &sen);
	if (ret) {
		printf("Failed to set it attribute ret: %d\n", ret);
	}
    
    // Set the sensor attribute for div4
	sen.val1 = paramGet_DIV4(valid);

	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &sen);
	if (ret) {
		printf("Failed to set div4 attribute ret: %d\n", ret);
	}

    // Set the sensor attribute for the gain
	sen.val1 = paramGet_GAIN(valid);
	ret = sensor_attr_set(this->m_dev, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &sen);
	if (ret) {
		printf("Failed to set gain attribute ret: %d\n", ret);
	}

    // Get the rate
	ret = sensor_sample_fetch(this->m_dev);
	if ((ret < 0) && (ret != -E2BIG)) {
		printf("sample update error. ret: %d\n", ret);
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

void LightSensor ::run_handler(FwIndexType portNum, U32 context) {
	Fw::Logic state;
	this->gpioRead_out(portNum, state);
	if (state == Fw::Logic::HIGH){ // port call to the gpio driver, pass in a specific pin # 
    	this->ReadData();
		this->tlmWrite_RawLightData(this->m_RawLightData);
		this->tlmWrite_IRLightData(this->m_IRLightData);
		this->tlmWrite_ALSLightData(this->m_ALSLightData);
	}
	
}

}  // namespace Components
