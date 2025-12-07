// ======================================================================
// \title  FileReadWrite.hpp
// \author t38talon
// \brief  hpp file for FileReadWrite component implementation class
// ======================================================================

#ifndef Components_FileReadWrite_HPP
#define Components_FileReadWrite_HPP

#include "FprimeZephyrReference/Components/FileReadWrite/FileReadWriteComponentAc.hpp"

namespace Components {

class FileReadWrite final : public FileReadWriteComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FileReadWrite object
    FileReadWrite(const char* const compName  //!< The component name
    );

    //! Destroy FileReadWrite object
    ~FileReadWrite();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for ReadRequest
    //!
    //! Port for requesting a file read operation
    //! Receives the filename to read from and sends to read output port
    void ReadRequest_handler(FwIndexType portNum,            //!< The port number
                             const Fw::StringBase& fileName  //!< The name of the file to read
                             ) override;

    //! Handler implementation for WriteRequest
    //!
    //! Port for requesting a file write operation
    //! Receives the filename and the data string to write
    void WriteRequest_handler(FwIndexType portNum,              //!< The port number
                              const Fw::StringBase& fileName,   //!< The name of the file to write
                              const Fw::StringBase& dataString  //!< Data string to write
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command WriteFile
    //!
    //! Command to write file data
    //! Takes file name and data, writes input to it (overrides what is in the file)
    //! Removed data: Fw.Buffer @< The data buffer to write to the filefp: note how to get buffer to accept command? or
    //! should we have diff functions for types of data? or just always write a U32?
    void WriteFile_cmdHandler(FwOpcodeType opCode,               //!< The opcode
                              U32 cmdSeq,                        //!< The command sequence number
                              const Fw::CmdStringArg& fileName,  //!< The name of the file to write
                              const Fw::CmdStringArg& toWrite    //!< The data string to write to the file
                              ) override;

    //! Handler implementation for command ReadFile
    //!
    //! Command to read file contents
    //! Reads file contents and returns what is in the file
    void ReadFile_cmdHandler(FwOpcodeType opCode,              //!< The opcode
                             U32 cmdSeq,                       //!< The command sequence number
                             const Fw::CmdStringArg& fileName  //!< The name of the file to read
                             ) override;

    //! Helper function to write file data
    //!
    //! Writes file data to the specified file path, overwriting existing content
    //! Returns true if the file was written successfully, false otherwise
    bool writeFile(const Fw::StringBase& fileName, const Fw::StringBase& dataString);
    bool readFile(const Fw::StringBase& fileName, Fw::Buffer& dataBuffer);

    static constexpr U32 CONFIG_MAX_READ_FILE_SIZE =
        256;  // ToDO maybe make this variable for the file.. right now were mostlu writing ints so its okay?
    U8 m_data[CONFIG_MAX_READ_FILE_SIZE];
};

}  // namespace Components

#endif
