module Components {
    @ Manages Files
    passive component FileReadWrite {

        ###############################################################################
        # Port Definitions
        ###############################################################################

        @ Port for requesting a file write operation
        @ Receives the filename and data string to write
        sync input port WriteRequest: FileReadWriteWriteRequest

        @ Port for requesting a file read operation
        @ Receives the filename to read from and sends to read output port
        sync input port ReadRequest: FileReadWriteReadRequest

        @ Port for returning file operation result
        @ Success or failure result of file operation (Fw bool)
        output port FileResult: FileReadWriteFileResult

        @ Port for returning read file data
        @ Result of the read operation containing file data (Fw Buffer)
        output port ReadResult: FileReadWriteReadResult

        ###############################################################################
        # Commands
        ###############################################################################

        @ Command to write file data
        @ Takes file name and data string, writes input to it (overrides what is in the file)
        @ Both strings are limited to 200 characters

        sync command WriteFile(
            fileName: string size FileNameStringSize @< The name of the file to write
            toWrite: string size 200 @< The data string to write to the file
        ) \

        @ Command to read file contents
        @ Reads file contents and returns what is in the file
        sync command ReadFile(
            fileName: string size FileNameStringSize @< The name of the file to read
        ) \

        ###############################################################################
        # Events
        ###############################################################################

        @ Event emitted when a file write operation fails
        @ Emitted when a file write operation fails with the reason
        event WriteFail(
            fileName: string size FileNameStringSize @< The name of the file that failed to write
        ) \
        severity warning high \
        id 0 \
        format "File {}: write failed "

        @ Event emitted when a file write operation completes successfully
        @ Emitted when a file write operation completes successfully
        event WriteSuccess(
            fileName: string size FileNameStringSize @< The name of the file that was written
        ) \
        severity activity high \
        id 1 \
        format "File write succeeded for {}"

        @ Event emitted when a file read operation fails
        @ Emitted when a file read operation fails with the reason
        event ReadFail(
            fileName: string size FileNameStringSize @< The name of the file that failed to read
        ) \
        severity warning high \
        id 2 \
        format "File read failed for {}"

        @ Event emitted when a file read operation completes successfully
        @ Emitted when a file read operation completes successfully
        event ReadSuccess(
            fileName: string size FileNameStringSize @< The name of the file that was read
        ) \
        severity activity high \
        id 3 \
        format "File read succeeded for {}"

        @ Event emitted with file contents
        @ Emits the contents of a file (truncated if larger than event string limit)
        event FileContents(
            fileName: string size FileNameStringSize @< The name of the file
            fileSize: U32 @< The size of the file in bytes
            contents: string size 200 @< The file contents (may be truncated)
        ) \
        severity activity high \
        id 4 \
        format "File contents for {} ({} bytes): {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }

    ###############################################################################
    # Port Type Definitions
    ###############################################################################

    @ Port type for file write request
    @ Receives the filename and one FW buffer containing data to write
    port FileReadWriteWriteRequest(
        fileName: string size FileNameStringSize @< The name of the file to write
        dataBuffer1: Fw.Buffer @< Data buffer
    )

    @ Port type for file read request
    @ Receives the filename to read from
    port FileReadWriteReadRequest(
        fileName: string size FileNameStringSize @< The name of the file to read
    )

    @ Port type for file operation result
    @ Returns success or failure status
    port FileReadWriteFileResult(
        success: Fw.Success @< Success or failure status
    )

    @ Port type for read result
    @ Returns the file data buffer
    port FileReadWriteReadResult(
        data: Fw.Buffer @< The file data buffer
    )
}
