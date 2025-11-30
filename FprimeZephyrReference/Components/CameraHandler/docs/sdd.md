# Components::CameraHandler

Passive component that handles camera specific payload capabilities.
  - Taking Images
  - Pinging

## Usage Examples
The camera handler can be commanded to take an image, after which it will forward a command to the PayloadCom component. It will then read in data from the PayloadCom until the image has finished sending.

### Typical Usage
Prior to taking a picture, the payload power loadswitch must be activated. Then "PING" the camera with the ping command. If the PING command returns successfully, then the camera is ready to take an image. 

## Port Descriptions
| Name | Description |
| commandOut | Command to forward to the PayloadCom component |
| dataIn | Data received from the PayloadCom component |

## Component States
Add component states in the chart below
| Name | Description |
| m_receiving | True when the camera is currently receiving image data |

## Commands
| Name | Description |
| PING | Send a ping to the camera - wait for a response |
| TAKE_IMAGE | Send "snap" command to the payload com component | 
| SEND_COMMAND | Send a user-specified command to the payload com component |

## Events
| Name | Description |
|---|---|
|---|---|

## Telemetry
| Name | Description |
|---|---|
|---|---|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
| CameraHandler-001 | The CameraHandler has a command to take an image. | Manual Test |
| CameraHandler-002 | The CameraHandler has a command to "ping" the camera. | Manual Test |
| CameraHandler-003 | The Camera Handler forwards the commands to the PayloadCom Component. | Manual Test |
| CameraHandler-004 | The Camera Handler receives all image data bytes and saves them to a new file. | Manual Test |

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |
