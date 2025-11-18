// ======================================================================
// \title  Logger.hpp
// \author aaron
// \brief  hpp file for Logger component implementation class
// ======================================================================

#ifndef Components_Logger_HPP
#define Components_Logger_HPP

#include "FprimeZephyrReference/Components/Logger/LoggerComponentAc.hpp"

namespace Components {

class Logger final : public LoggerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Logger object
    Logger(const char* const compName  //!< The component name
    );

    //! Destroy Logger object
    ~Logger();

  private:

    // creates new log directory 
    void _createDirectory();

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------
    
    //! Handler implementation for serialGet
    //!
    //! Port for taking in serial data
    void serialGet_handler(FwIndexType portNum,  //!< The port number
                           const Fw::ComBuffer& serialData) override;
};

}  // namespace Components

#endif
