// ======================================================================
// \title  StartupManager.cpp
// \author starchmd
// \brief  cpp file for StartupManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/StartupManager/StartupManager.hpp"

#include "Os/File.hpp"
#include <zephyr/drivers/rtc.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StartupManager ::StartupManager(const char* const compName) : StartupManagerComponentBase(compName) {}

StartupManager ::~StartupManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

//! \brief Template function to read a type T from a file at file_path
//!
//! This will read a type T with size 'size' from the file located at file_path. It will return SUCCESS if
//! the read and deserialization were successful, and FAILURE otherwise.
//!
//! The file will be opened and closed within this function. value will not be modified by this function unless
//! the read operation is successful.
//!
//! \warning this function is only safe to use for types T with size `size` that fit well in stack memory.
//!
//! \param file_path: path to the file to read from
//! \param value: reference to the variable to read into
//! \return Status of the read operation
template <typename T, FwSizeType BUFFER_SIZE>
StartupManager::Status read(const Fw::StringBase& file_path, T& value) {
    // Create the necessary file and deserializer objects for reading a type from a file
    StartupManager::Status return_status = StartupManager::FAILURE;
    Os::File file;
    U8 data_buffer[BUFFER_SIZE];
    Fw::ExternalSerializeBuffer deserializer(data_buffer, sizeof(data_buffer));

    // Open the file for reading, and continue only if successful
    Os::File::Status status = file.open(file_path.toChar(), Os::File::OPEN_READ);
    if (status == Os::File::OP_OK) {
        FwSizeType size = sizeof(data_buffer);
        status = file.read(data_buffer, size);
        if (status == Os::File::OP_OK && size == sizeof(data_buffer)) {
            // When the read is successful, and the size is correct then the buffer must absolutely contain the
            // serialized data and thus it is safe to assert on the deserialization status
            deserializer.setBuffLen(size);
            Fw::SerializeStatus serialize_status = deserializer.deserializeTo(value);
            FW_ASSERT(serialize_status == Fw::SerializeStatus::FW_SERIALIZE_OK,
                      static_cast<FwAssertArgType>(serialize_status));
            return_status = StartupManager::SUCCESS;
        }
    }
    (void)file.close();
    return return_status;
}

//! \brief Template function to write a type T to a file at file_path
//!
//! This will write a type T with size 'size' to the file located at file_path. It will return SUCCESS if
//! the serialization and write were successful, and FAILURE otherwise.
//!
//! The file will be opened and closed within this function.
//!
//! \warning this function is only safe to use for types T with size `size` that fit well in stack memory.
//!
//! \param file_path: path to the file to write to
//! \param value: reference to the variable to write
//! \return Status of the write operation
template <typename T, FwSizeType BUFFER_SIZE>
StartupManager::Status write(const Fw::StringBase& file_path, const T& value) {
    // Create the necessary file and deserializer objects for reading a type from a file
    StartupManager::Status return_status = StartupManager::FAILURE;
    Os::File file;
    U8 data_buffer[BUFFER_SIZE];

    // Serialize the value into the data buffer. Since the buffer is created here it is safe to assert on the
    // serialization status.
    Fw::ExternalSerializeBuffer serializer(data_buffer, sizeof(data_buffer));
    Fw::SerializeStatus serialize_status = serializer.serializeFrom(value);
    FW_ASSERT(serialize_status == Fw::SerializeStatus::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(serialize_status));

    // Open the file for writing, and continue only if successful
    Os::File::Status status = file.open(file_path.toChar(), Os::File::OPEN_CREATE, Os::File::OVERWRITE);
    if (status == Os::File::OP_OK) {
        FwSizeType size = sizeof(data_buffer);
        status = file.write(data_buffer, size);
        if (status == Os::File::OP_OK && size == sizeof(data_buffer)) {
            return_status = StartupManager::SUCCESS;
        }
    }
    (void)file.close();
    return return_status;
}

FwSizeType StartupManager ::update_boot_count() {
    // Read the boot count file path from parameter and assert that it is either valid or the default value
    FwSizeType boot_count = 0;
    Fw::ParamValid is_valid;
    auto boot_count_file = this->paramGet_BOOT_COUNT_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Open the boot count file and add one to the current boot count ensuring a minimum of 1 in the case
    // of read failure. Since read will retain the `0` initial value on read failure, we can ignore the error
    // status returned by the read.
    (void)read<FwSizeType, sizeof(FwSizeType)>(boot_count_file, boot_count);
    boot_count = FW_MAX(1, boot_count + 1);
    // Rewrite the updated boot count back to the file, and on failure emit a warning about the inability to
    // persist the boot count.
    StartupManager::Status status = write<FwSizeType, sizeof(FwSizeType)>(boot_count_file, boot_count);
    if (status != StartupManager::SUCCESS) {
        this->log_WARNING_LO_BootCountUpdateFailure();
    }
    return boot_count;
}

Fw::Time StartupManager ::update_quiescence_start() {
    Fw::ParamValid is_valid;
    auto time_file = this->paramGet_QUIESCENCE_START_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    Fw::Time time = this->getTime();
    // Open the quiescence start time file and read the current time. On read failure, return the current time.
    StartupManager::Status status = read<Fw::Time, Fw::Time::SERIALIZED_SIZE>(time_file, time);
    // On read failure, write the current time to the file for future reads. This only happens on read failure because
    // there is a singular quiescence start time for the whole mission.
    if (status != StartupManager::SUCCESS) {
        status = write<Fw::Time, Fw::Time::SERIALIZED_SIZE>(time_file, time);
        if (status != StartupManager::SUCCESS) {
            this->log_WARNING_LO_QuiescenceFileInitFailure();
        }
    }
    return time;
}

Fw::Time StartupManager ::get_uptime() {
    uint32_t seconds = k_uptime_seconds();
    Fw::Time time(TimeBase::TB_PROC_TIME, 0, static_cast<U32>(seconds), 0);
    return time;
}

void StartupManager ::completeSequence_handler(FwIndexType portNum,
                                               FwOpcodeType opCode,
                                               U32 cmdSeq,
                                               const Fw::CmdResponse& response) {
    // Respond to the completion status of the start-up sequence
    if (response == Fw::CmdResponse::OK) {
        this->log_ACTIVITY_LO_StartupSequenceFinished();
    } else {
        this->log_WARNING_LO_StartupSequenceFailed(response);
    }
}

void StartupManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::ParamValid is_valid;

    // On the first call, update the boot count, set the quiescence start time, and dispatch the start-up sequence
    if (this->m_boot_count == 0) {
        this->m_boot_count = this->update_boot_count();
        this->m_quiescence_start = this->update_quiescence_start();

        Fw::ParamString first_sequence = this->paramGet_STARTUP_SEQUENCE_FILE(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
        this->runSequence_out(0, first_sequence);
    }

    // Calculate the quiescence end time based on the quiescence period parameter
    Fw::TimeIntervalValue quiescence_period = this->paramGet_QUIESCENCE_TIME(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
    Fw::Time quiescence_interval(this->m_quiescence_start.getTimeBase(), quiescence_period.get_seconds(),
                                 quiescence_period.get_useconds());
    Fw::Time end_time = Fw::Time::add(this->m_quiescence_start, quiescence_interval);

    // Are we waiting for quiescence?
    if (this->m_waiting) {
        // Check if the system is armed or if this is not the first boot. In both cases, we skip waiting.
        bool armed = this->paramGet_ARMED(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

        Fw::Time current_time =
            (end_time.getTimeBase() == TimeBase::TB_PROC_TIME) ? this->get_uptime() : this->getTime();

        // If not armed or this is not the first boot, we skip waiting
        if (!armed || end_time <= current_time) {
            this->m_waiting = false;
            this->cmdResponse_out(this->m_stored_opcode, this->m_stored_sequence, Fw::CmdResponse::OK);
        }
    }
    this->tlmWrite_QuiescenceEndTime(
        Fw::TimeValue(end_time.getTimeBase(), end_time.getContext(), end_time.getSeconds(), end_time.getUSeconds()));
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
