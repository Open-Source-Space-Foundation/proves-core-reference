// ======================================================================
// \title  StartupManager.cpp
// \author starchmd
// \brief  cpp file for StartupManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/StartupManager/StartupManager.hpp"
#include "Os/File.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StartupManager ::StartupManager(const char* const compName) : StartupManagerComponentBase(compName) {}

StartupManager ::~StartupManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

FwSizeType StartupManager ::update_boot_count() {
    // Read the boot count file path from parameter and assert that it is either valid or the default value
    Fw::ParamValid is_valid;
    auto boot_count_file = this->paramGet_BOOT_COUNT_FILE(0, is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Open the boot count file and read the current boot count
    FwSizeType boot_count = 0;
    Os::File file;
    Os::File::Status status = file.open(boot_count_file.toChar(), Os::File::OPEN_READ);
    U8 buffer[sizeof(FwSizeType)];

    // If the file read ok, then we read the boot count. Otherwise, we assume boot count is zero thus making
    // this the first boot.
    if (status == Os::File::OP_OK) {
        FwSizeType size = sizeof(buffer);
        status = file.read(buffer, size);
        if (status == Os::File::OP_OK) {
            Fw::ExternalSerializeBuffer buffer_obj(buffer, sizeof(buffer));
            Fw::SerializeStatus serialization_status = buffer_obj.deserializeTo(boot_count);
            if (serialization_status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
                boot_count = 0;  // Default to zero if deserialization fails
            }
        }
        file.close();
    }
    boot_count = boot_count + 1;

    // Open the file for writing the boot count
    status = file.open(boot_count_file.toChar(), Os::File::OPEN_CREATE, Os::File::OVERWRITE);
    if (status == Os::File::OP_OK) {
        Fw::ExternalSerializeBuffer buffer_obj(buffer, sizeof(FwSizeType));
        Fw::SerializeStatus serialize_status = buffer_obj.serializeFrom(boot_count);
        // Write only when the serialization was successful
        if (serialize_status == Fw::SerializeStatus::FW_SERIALIZE_OK) {
            FwSizeType size = sizeof(buffer);
            (void)file.write(buffer, size);
        }
        file.close();
    }
    return boot_count;
}

void StartupManager ::run_handler(FwIndexType portNum, U32 context) {
    // On the first call, update the boot count
    if (this->m_boot_count == 0) {
        this->m_boot_count = this->update_boot_count();
    }
    Fw::ParamValid is_valid;
    while (this->m_boot_count == 1 && armed) {
        armed = this->paramGet_ARMED(is_valid);
    }

    char buffer[Fw::TimeIntervalValue::SERIALIZED_SIZE];

    Fw::ParamString bootCount =

        Os::File file;

    Os::File::Status status = file.open(, Os::File::OPEN_READ);

    this->cmdResponse_out(this->m_stored_opcode, this->m_stored_sequence, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StartupManager ::WAIT_FOR_QUIESCENCE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_stored_opcode = opCode;
    this->m_stored_sequence = cmdSeq;
}

}  // namespace Components
