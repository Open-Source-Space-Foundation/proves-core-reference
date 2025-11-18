// ======================================================================
// \title  Logger.cpp
// \author aaron
// \brief  cpp file for Logger component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Logger/Logger.hpp"
#include "Os/FileSystem.hpp"
#include "Os/File.hpp"

// namespace fs = Os::FileInterface
namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Logger ::Logger(const char* const compName) : LoggerComponentBase(compName) {}

Logger ::~Logger() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Logger ::serialGet_handler(FwIndexType portNum, const Fw::ComBuffer& serialData) {
    // TODO
    // bool exists = Os::FileSystem::exists("/telemetryLogs");
    // if (exists == false) {
    //     Logger::_createDirectory();
    // }

    // Os::FileInterface::Status status = Os::FileInterface::open("/teletmetryLogs/telemtry.log", Os::FileInterface::OPEN_CREATE, Os::FileInterface::NO_OVERWRITE);
    // if (status == Os::FileInterface::OP_OK) {
    //     Os::FileInterface file;
    //     // file.write()
    // }

}

void Logger::_createDirectory() {
    // Os::FileSystem::Status status = Os::FileSystem::createDirectory("/telemeryLogs");
}


}  // namespace Components
