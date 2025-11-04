// ======================================================================
// \title  DistanceSensorManager.hpp
// \brief  hpp file for DistanceSensorManager component implementation class
// ======================================================================

#ifndef Components_DistanceSensorManager_HPP
#define Components_DistanceSensorManager_HPP

// clang-format off
// Keep the includes in this order
#include "FprimeZephyrReference/Components/Drv/DistanceSensorManager/DistanceSensorManagerComponentAc.hpp"
#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"
// clang-format on

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

namespace Drv {

// VL6180 Register Definitions
#define VL6180_REG_IDENTIFICATION_MODEL_ID          0x0000
#define VL6180_REG_SYSTEM_INTERRUPT_CONFIG          0x0014
#define VL6180_REG_SYSTEM_INTERRUPT_CLEAR           0x0015
#define VL6180_REG_SYSTEM_FRESH_OUT_OF_RESET        0x0016
#define VL6180_REG_SYSRANGE_START                   0x0018
#define VL6180_REG_SYSRANGE_INTERMEASUREMENT_PERIOD 0x001B
#define VL6180_REG_SYSRANGE_VHV_REPEAT_RATE         0x0031
#define VL6180_REG_SYSRANGE_VHV_RECALIBRATE         0x002E
#define VL6180_REG_SYSRANGE_RANGE_CHECK_ENABLES     0x002D
#define VL6180_REG_RESULT_RANGE_STATUS              0x004D
#define VL6180_REG_RESULT_RANGE_VAL                 0x0062

// VL6180 Constants
#define VL6180_MODEL_ID                             0xB4
#define VL6180_RANGE_START_SINGLE_SHOT              0x01

class DistanceSensorManager final : public DistanceSensorManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DistanceSensorManager object
    DistanceSensorManager(const char* const compName);

    //! Destroy DistanceSensorManager object
    ~DistanceSensorManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Get the distance reading from the VL6180 sensor
    F64 distanceGet_handler(const FwIndexType portNum  //!< The port number
                           ) override;

    //! Handler for run port (called by rate group)
    void run_handler(const FwIndexType portNum,  //!< The port number
                     U32 context                 //!< The call order
                    ) override;

    // ----------------------------------------------------------------------
    // Command handler implementations
    // ----------------------------------------------------------------------

    //! Handler for READ_DISTANCE command
    void READ_DISTANCE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq            //!< The command sequence number
                                 ) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Initialize the VL6180 sensor
    //! \return true if initialization succeeded, false otherwise
    bool initializeSensor();

    //! Read a byte from VL6180 register
    //! \param reg Register address (16-bit)
    //! \param value Pointer to store the read value
    //! \return 0 on success, negative error code on failure
    int readRegister(uint16_t reg, uint8_t* value);

    //! Write a byte to VL6180 register
    //! \param reg Register address (16-bit)
    //! \param value Value to write
    //! \return 0 on success, negative error code on failure
    int writeRegister(uint16_t reg, uint8_t value);

    //! Start a single-shot range measurement
    //! \return 0 on success, negative error code on failure
    int startRangeMeasurement();

    //! Read the range result
    //! \param distance_mm Pointer to store distance in millimeters
    //! \return 0 on success, negative error code on failure
    int readRangeResult(uint8_t* distance_mm);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr I2C device spec for communicating with VL6180
    struct i2c_dt_spec i2c_spec;

    //! Flag indicating whether sensor is initialized
    bool initialized;
};

}  // namespace Drv

#endif
