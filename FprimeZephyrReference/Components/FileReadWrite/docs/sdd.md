# Components::FileReadWrite

Manages Files to ensure every component creates files in the same way


## Port Descriptions
| Name | Description | Type |
|---|---|---|
| WriteRequest | Receives the filename and the data to be sent through the file (two FW buffers) | Input |
| ReadRequest | Receives the filename to read from and sends to read output port | Input |
| FileResult | Success or failure result of file operation (Fw bool) | Output |
| ReadResult | Result of the read operation containing file data (Fw Buffer) | Output |

## Commands
| Name | Description |
|---|---|
| WriteFile | Takes file name and data, writes input to it (overrides what is in the file) |
| ReadFile | Reads file contents and returns what is in the file |

## Events
| Name | Description |
|---|---|
| WriteFail | Emitted when a file write operation fails with the reason |
| WriteSuccess | Emitted when a file write operation completes successfully |
| ReadFail | Emitted when a file read operation fails with the reason |
| ReadSuccess | Emitted when a file read operation completes successfully |


## Requirements
| Name | Description | Validation |
|---|---|---|
| FM-001 | The component shall accept read and write requests from other comps through ports |
| FM-002 | The component shall provide output ports that notify the component of the needed data and the success/fail of the file operation | Verify port accepts filename parameter |
| FM-003 | The component shall write file data to the specified file path, overwriting existing content | Test write operation overwrites existing file content |
|shall return read file data via ReadResult output port | Verify ReadResult port contains file data buffer |
| FM-004 | The component shall handle file operations in a thread-safe manner | Verify concurrent access does not corrupt file operations |
| FM-005 | The component shall create files if the files do not exist | Test invalid paths result in appropriate error events |

## Change Log
| Date | Description |
|---|---|
| --- | Initial Draft |
