// ======================================================================
// \title  Helpers.cpp
// \brief  cpp file for Helpers component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

F64 sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + (val.val2 / 1000000.0f);
}

}  // namespace Drv
