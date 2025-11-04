// ======================================================================
// \title  DistanceSensorManager.cpp
// \brief  cpp file for DistanceSensorManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/DistanceSensorManager/DistanceSensorManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DistanceSensorManager::DistanceSensorManager(const char* const compName)
    : DistanceSensorManagerComponentBase(compName),
      i2c_spec(I2C_DT_SPEC_GET(DT_NODELABEL(vl6180))),
      initialized(false) {
    // Check if I2C device is ready
    if (!i2c_is_ready_dt(&i2c_spec)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    // Initialize the sensor
    if (!initializeSensor()) {
        this->log_WARNING_HI_InitializationFailed();
        return;
    }

    initialized = true;
}

DistanceSensorManager::~DistanceSensorManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F64 DistanceSensorManager::distanceGet_handler(FwIndexType portNum) {
    if (!initialized || !i2c_is_ready_dt(&i2c_spec)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0.0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    // Start a single-shot range measurement
    if (startRangeMeasurement() != 0) {
        this->log_WARNING_HI_I2cError();
        return 0.0;
    }

    // Wait for measurement to complete (typical: ~10ms for single-shot)
    k_msleep(30);

    // Read the range result
    uint8_t distance_mm = 0;
    if (readRangeResult(&distance_mm) != 0) {
        this->log_WARNING_HI_I2cError();
        return 0.0;
    }
    this->log_WARNING_HI_I2cError_ThrottleClear();

    // Convert to F64 and send telemetry
    F64 distance = static_cast<F64>(distance_mm);
    this->tlmWrite_Distance(distance);

    return distance;
}

void DistanceSensorManager::run_handler(FwIndexType portNum, U32 context) {
    // Trigger a distance measurement
    this->distanceGet_handler(0);
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void DistanceSensorManager::READ_DISTANCE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Trigger a distance measurement
    F64 distance = this->distanceGet_handler(0);

    // Send event with the reading
    this->log_ACTIVITY_LO_DistanceReading(distance);

    // Send command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool DistanceSensorManager::initializeSensor() {
    uint8_t model_id = 0;

    // Read model ID to verify communication
    if (readRegister(VL6180_REG_IDENTIFICATION_MODEL_ID, &model_id) != 0) {
        return false;
    }

    if (model_id != VL6180_MODEL_ID) {
        return false;
    }

    // Check if sensor needs initialization (fresh out of reset)
    uint8_t fresh_out_of_reset = 0;
    if (readRegister(VL6180_REG_SYSTEM_FRESH_OUT_OF_RESET, &fresh_out_of_reset) != 0) {
        return false;
    }

    if (fresh_out_of_reset == 0x01) {
        // Perform mandatory private registers initialization (from VL6180 datasheet)
        // These are required settings from ST's application note
        writeRegister(0x0207, 0x01);
        writeRegister(0x0208, 0x01);
        writeRegister(0x0096, 0x00);
        writeRegister(0x0097, 0xFD);
        writeRegister(0x00E3, 0x00);
        writeRegister(0x00E4, 0x04);
        writeRegister(0x00E5, 0x02);
        writeRegister(0x00E6, 0x01);
        writeRegister(0x00E7, 0x03);
        writeRegister(0x00F5, 0x02);
        writeRegister(0x00D9, 0x05);
        writeRegister(0x00DB, 0xCE);
        writeRegister(0x00DC, 0x03);
        writeRegister(0x00DD, 0xF8);
        writeRegister(0x009F, 0x00);
        writeRegister(0x00A3, 0x3C);
        writeRegister(0x00B7, 0x00);
        writeRegister(0x00BB, 0x3C);
        writeRegister(0x00B2, 0x09);
        writeRegister(0x00CA, 0x09);
        writeRegister(0x0198, 0x01);
        writeRegister(0x01B0, 0x17);
        writeRegister(0x01AD, 0x00);
        writeRegister(0x00FF, 0x05);
        writeRegister(0x0100, 0x05);
        writeRegister(0x0199, 0x05);
        writeRegister(0x01A6, 0x1B);
        writeRegister(0x01AC, 0x3E);
        writeRegister(0x01A7, 0x1F);
        writeRegister(0x0030, 0x00);

        // Clear fresh_out_of_reset flag
        writeRegister(VL6180_REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00);
    }

    // Configure range measurement settings
    writeRegister(VL6180_REG_SYSRANGE_VHV_REPEAT_RATE, 0xFF);
    writeRegister(VL6180_REG_SYSRANGE_INTERMEASUREMENT_PERIOD, 0x00);

    return true;
}

int DistanceSensorManager::readRegister(uint16_t reg, uint8_t* value) {
    // VL6180 uses 16-bit register addresses (big-endian)
    uint8_t reg_addr[2];
    reg_addr[0] = (reg >> 8) & 0xFF;  // MSB
    reg_addr[1] = reg & 0xFF;         // LSB

    // Write register address then read the value
    int ret = i2c_write_read_dt(&i2c_spec, reg_addr, 2, value, 1);

    return ret;
}

int DistanceSensorManager::writeRegister(uint16_t reg, uint8_t value) {
    // VL6180 uses 16-bit register addresses (big-endian)
    uint8_t buf[3];
    buf[0] = (reg >> 8) & 0xFF;  // Register MSB
    buf[1] = reg & 0xFF;         // Register LSB
    buf[2] = value;              // Data

    int ret = i2c_write_dt(&i2c_spec, buf, 3);

    return ret;
}

int DistanceSensorManager::startRangeMeasurement() {
    // Clear any interrupts
    writeRegister(VL6180_REG_SYSTEM_INTERRUPT_CLEAR, 0x07);

    // Start single-shot range measurement
    return writeRegister(VL6180_REG_SYSRANGE_START, VL6180_RANGE_START_SINGLE_SHOT);
}

int DistanceSensorManager::readRangeResult(uint8_t* distance_mm) {
    uint8_t status = 0;
    int timeout = 100;  // Maximum wait cycles

    // Poll for measurement ready (bit 2 of interrupt status)
    while (timeout-- > 0) {
        if (readRegister(VL6180_REG_RESULT_RANGE_STATUS, &status) != 0) {
            return -1;
        }

        // Check if new sample is ready (bit 2)
        if ((status & 0x04) != 0) {
            break;
        }

        k_msleep(1);
    }

    if (timeout <= 0) {
        return -2;  // Timeout
    }

    // Read the range value
    if (readRegister(VL6180_REG_RESULT_RANGE_VAL, distance_mm) != 0) {
        return -3;
    }

    // Clear interrupts
    writeRegister(VL6180_REG_SYSTEM_INTERRUPT_CLEAR, 0x07);

    return 0;
}

}  // namespace Drv
