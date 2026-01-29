// ======================================================================
// \title  FsFormat.hpp
// \author nate
// \brief  hpp file for FsFormat component implementation class
// ======================================================================

#ifndef Components_FsFormat_HPP
#define Components_FsFormat_HPP

#include "FprimeZephyrReference/Components/FsFormat/FsFormatComponentAc.hpp"

namespace Components {

class FsFormat final : public FsFormatComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FsFormat object
    FsFormat(const char* const compName  //!< The component name
    );

    //! Destroy FsFormat object
    ~FsFormat();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the file system format component
    void configure(const int partition_id  //!< The partition ID to format
    );

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command FORMAT
    //!
    //! Command to format the file system
    //!
    //! Use at your own risk! This will erase all data on the storage partition
    //! and will cause the system to fatal.
    void FORMAT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                           U32 cmdSeq            //!< The command sequence number
                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    int m_partition_id;  //!< The partition ID format
};

}  // namespace Components

#endif
