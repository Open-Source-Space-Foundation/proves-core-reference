// ======================================================================
// \title  Helpers.hpp
// \brief  hpp file for Helpers component implementation class
// ======================================================================

#ifndef Components_Helpers_HPP
#define Components_Helpers_HPP

#include <Fw/Types/BasicTypes.h>
#include <zephyr/drivers/sensor.h>

namespace Drv {

//! Convert a Zephyr sensor_value to an Fprime F64
F64 sensor_value_to_f64(const struct sensor_value& val);

}  // namespace Drv

#endif
