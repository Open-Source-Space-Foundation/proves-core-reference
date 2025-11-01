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
    auto boot_count_file = this->paramGet_BOOT_COUNT_FILE(is_valid);
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
    // Boot count of zero is a flag value, so ensure a minimum boot count of 1
    boot_count = FW_MAX(1, boot_count + 1);

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

Fw::Time StartupManager ::get_quiescence_start() {
    // Read the quiescence start time file path from parameter and assert that it is either valid or the default value
    Fw::ParamValid is_valid;

    auto time_file = this->paramGet_QUIESCENCE_START_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Open the boot count file and read the current boot count
    Fw::Time time;
    Os::File file;
    Os::File::Status status = file.open(time_file.toChar(), Os::File::OPEN_READ);
    U8 buffer[Fw::Time::SERIALIZED_SIZE];

    // If the file read ok, then we read the quiescence start time.
    if (status == Os::File::OP_OK) {
        FwSizeType size = sizeof(buffer);
        status = file.read(buffer, size);
        if (status == Os::File::OP_OK) {
            Fw::ExternalSerializeBuffer buffer_obj(buffer, sizeof(buffer));
            Fw::SerializeStatus serialization_status = buffer_obj.deserializeTo(time);
            if (serialization_status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
                time = this->getTime();  // Default to current time if deserialization fails
            }
        }
        (void)file.close();
    }
    // Write quiescence start time if read failed
    if (status != Os::File::OP_OK) {
        FwSizeType size = sizeof(buffer);
        time = this->getTime();
        status = file.open(time_file.toChar(), Os::File::OPEN_CREATE, Os::File::OVERWRITE);
        if (status == Os::File::OP_OK) {
            Fw::ExternalSerializeBuffer buffer_obj(buffer, sizeof(Fw::Time::SERIALIZED_SIZE));
            Fw::SerializeStatus serialize_status = buffer_obj.serializeFrom(time);
            if (serialize_status == Fw::SerializeStatus::FW_SERIALIZE_OK) {
                (void)file.write(buffer, size);
            }
        }
        (void)file.close();
    }
    return time;
}

void StartupManager ::completeSequence_handler(FwIndexType portNum,
                                               FwOpcodeType opCode,
                                               U32 cmdSeq,
                                               const Fw::CmdResponse& response) {
    // TODO
}

void StartupManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::ParamValid is_valid;

    // On the first call, update the boot count, set the quiescence start time, and dispatch the start-up sequence
    if (this->m_boot_count == 0) {
        this->m_boot_count = this->update_boot_count();
        this->m_quiescence_start = this->get_quiescence_start();

        Fw::ParamString first_sequence = this->paramGet_STARTUP_SEQUENCE_FILE(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
        this->runSequence_out(0, first_sequence);
    }

    // Are we waiting for quiescence?
    if (this->m_waiting) {
        // Check if the system is armed or if this is not the first boot. In both cases, we skip waiting.
        bool armed = this->paramGet_ARMED(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

        Fw::TimeIntervalValue quiescence_period = this->paramGet_QUIESCENCE_TIME(is_valid);
        Fw::Time quiescence_interval(quiescence_period.get_seconds(), quiescence_period.get_useconds());
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
        Fw::Time end_time = Fw::Time::add(this->m_quiescence_start, quiescence_interval);

        // If not armed or this is not the first boot, we skip waiting
        if (!armed || end_time <= this->getTime()) {
            this->m_waiting = false;
            this->cmdResponse_out(this->m_stored_opcode, this->m_stored_sequence, Fw::CmdResponse::OK);
        }
    }
    this->tlmWrite_BootCount(this->m_boot_count);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StartupManager ::WAIT_FOR_QUIESCENCE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_stored_opcode = opCode;
    this->m_stored_sequence = cmdSeq;
    this->m_waiting = true;
}

}  // namespace Components
