// ======================================================================
// \title  FileReadWrite.cpp
// \author t38talon
// \brief  cpp file for FileReadWrite component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/FileReadWrite/FileReadWrite.hpp"

#include "Fw/Log/LogString.hpp"
#include "Fw/Types/String.hpp"
#include "Os/File.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FileReadWrite ::FileReadWrite(const char* const compName) : FileReadWriteComponentBase(compName) {}

FileReadWrite ::~FileReadWrite() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void FileReadWrite ::ReadRequest_handler(FwIndexType portNum, const Fw::StringBase& fileName) {
    Fw::Buffer dataBuffer;

    // Lock mutex to protect m_data from concurrent access
    // Keep locked until after we send the buffer to prevent data from being overwritten
    Os::ScopeLock lock(this->m_readMutex);

    bool success = this->readFileUnlocked(fileName, dataBuffer);
    if (success) {
        this->log_ACTIVITY_HI_ReadSuccess(fileName);
    }

    // send the success or failure to the FileResult port
    Fw::Success result = success ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
    this->FileResult_out(portNum, result);

    // send the file data to the ReadResult port (only if read was successful)
    // Mutex remains locked here to ensure m_data isn't overwritten before the buffer is sent
    if (success) {
        this->ReadResult_out(portNum, dataBuffer);
    }
    // Mutex is automatically unlocked when lock goes out of scope
    // At this point, the buffer has been sent, so it's safe for another thread to use m_data
}

void FileReadWrite ::WriteRequest_handler(FwIndexType portNum,
                                          const Fw::StringBase& fileName,
                                          const Fw::StringBase& dataString) {
    bool success = this->writeFile(fileName, dataString);
    if (success) {
        this->log_ACTIVITY_HI_WriteSuccess(fileName);
    }

    // send the success or failure to the FileResult port
    Fw::Success result = success ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
    this->FileResult_out(portNum, result);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FileReadWrite ::WriteFile_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          const Fw::CmdStringArg& fileName,
                                          const Fw::CmdStringArg& toWrite) {
    // Note: WriteFile command doesn't take a buffer parameter so using an F Prime command string argument for
    // everything here
    bool success = this->writeFile(fileName, toWrite);
    if (success) {
        this->log_ACTIVITY_HI_WriteSuccess(fileName);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void FileReadWrite ::ReadFile_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& fileName) {
    Fw::Buffer dataBuffer;

    // Lock mutex to protect m_data from concurrent access
    // Keep locked until after we copy the data to prevent it from being overwritten
    Os::ScopeLock lock(this->m_readMutex);

    bool success = this->readFileUnlocked(fileName, dataBuffer);
    if (success) {
        this->log_ACTIVITY_HI_ReadSuccess(fileName);
        // Event that returns the contents of the file (written to databuffer)
        // Create a string from the buffer data (truncate to max size for event)
        // Mutex remains locked here to ensure m_data isn't overwritten during the copy
        const FwSizeType maxContentSize = CONFIG_MAX_READ_FILE_SIZE;
        char contentBuffer[kMaxContentBufferSize];  // Compile-time constant size (+1 for null terminator)
        FwSizeType contentSize = (dataBuffer.getSize() < maxContentSize) ? dataBuffer.getSize() : maxContentSize;

        // Copy buffer data into fixed-size buffer (handles binary data safely)
        std::memcpy(contentBuffer, dataBuffer.getData(), static_cast<size_t>(contentSize));
        contentBuffer[contentSize] = '\0';  // Null terminate

        // Use Fw::String (fixed-size, compile-time allocated) instead of std::string
        Fw::String contentStr(contentBuffer);
        Fw::LogStringArg contents(contentStr.toChar());
        this->log_ACTIVITY_HI_FileContents(fileName, dataBuffer.getSize(), contents);
    }
    // Mutex is automatically unlocked when lock goes out of scope
    // At this point, the data has been copied, so it's safe for another thread to use m_data
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

bool FileReadWrite ::writeFile(const Fw::StringBase& file_name, const Fw::StringBase& dataString) {
    // Open file with CREATE mode (creates file if it doesn't exist, truncates if it does)
    Os::File file;
    Os::File::Status open_status = file.open(file_name.toChar(), Os::File::OPEN_CREATE);
    if (open_status != Os::File::Status::OP_OK) {
        this->log_WARNING_HI_WriteFail(file_name);
        return false;
    }

    // Write buffer - need to store size in a variable since write() takes a non-const reference
    // On entry, writeSize is the number of bytes to write
    // On return, writeSize is the number of bytes actually written
    FwSizeType expectedSize = static_cast<FwSizeType>(dataString.length());
    FwSizeType writeSize = expectedSize;
    const U8* data = reinterpret_cast<const U8*>(dataString.toChar());

    // Write string data to file
    Os::File::Status write_status = file.write(data, writeSize, Os::File::WaitType::WAIT);

    // Check both write status and that all bytes were written
    if ((write_status != Os::File::Status::OP_OK) || (writeSize != expectedSize)) {
        file.close();
        this->log_WARNING_HI_WriteFail(file_name);
        return false;
    }

    file.close();
    return true;
}

bool FileReadWrite ::readFileUnlocked(const Fw::StringBase& fileName, Fw::Buffer& dataBuffer) {
    // NOTE: Caller must hold m_readMutex lock before calling this function
    // This function does NOT lock the mutex - it assumes the caller has already locked it

    // Open file for reading
    Os::File file;
    Os::File::Status open_status = file.open(fileName.toChar(), Os::File::OPEN_READ);
    if (open_status != Os::File::Status::OP_OK) {
        this->log_WARNING_HI_ReadFail(fileName);
        return false;
    }

    // Read into fixed-size member buffer (compile-time allocated, no dynamic allocation)
    FwSizeType readSize = CONFIG_MAX_READ_FILE_SIZE;
    Os::File::Status read_status = file.read(this->m_data, readSize, Os::File::WaitType::WAIT);
    file.close();

    if (read_status != Os::File::Status::OP_OK) {
        this->log_WARNING_HI_ReadFail(fileName);
        return false;
    }

    // Set the buffer to point to our fixed-size member data (no dynamic allocation)
    // The mutex is held by the caller, ensuring m_data is not overwritten
    dataBuffer.set(this->m_data, readSize, 0);
    return true;
}

}  // namespace Components
