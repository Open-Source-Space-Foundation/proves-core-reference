// ======================================================================
// \title  TlmArchive.cpp
// \author aychar
// \brief  cpp file for TlmArchive component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchive.hpp"

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

namespace {

constexpr const char* TLM_DIRECTORY = "//tlm";
constexpr const char* PRE_DEPLOYMENT_TLM_PATH = "//tlm/pre_deployment.tlm";

}  // namespace

TlmArchive ::TlmArchive(const char* const compName) : TlmArchiveComponentBase(compName) {}

TlmArchive ::~TlmArchive() {}

void TlmArchive::comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    (void)portNum;
    (void)context;

    if (this->deploymentStateGet_out(0)) {
        return;
    }

    if (Os::FileSystem::createDirectory(TLM_DIRECTORY, false) != Os::FileSystem::OP_OK) {
        return;
    }

    Os::File file;
    if (file.open(PRE_DEPLOYMENT_TLM_PATH, Os::File::OPEN_APPEND) != Os::File::OP_OK) {
        return;
    }

    FwSizeType size = data.getSize();
    (void)file.write(data.getBuffAddr(), size);
    file.close();
}

}  // namespace Components
