// ======================================================================
// \title  RAM.hpp
// \author t38talon
// \brief  hpp file for RAM component implementation class
// ======================================================================

#ifndef Components_RAM_HPP
#define Components_RAM_HPP

#include "FprimeZephyrReference/Components/RAM/RAMComponentAc.hpp"

namespace Components {

class RAM final : public RAMComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct RAM object
    RAM(const char* const compName  //!< The component name
    );

    //! Destroy RAM object
    ~RAM();

    int init();
    int read(uint32_t offset, uint8_t* data, size_t len);
    int write(uint32_t offset, const uint8_t* data, size_t len);
    int read_id(uint8_t* id, size_t len);
};

}  // namespace Components

#endif
