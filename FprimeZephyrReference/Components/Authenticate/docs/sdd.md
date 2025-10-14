# Components::Authenticate

The Authenticator component verifies the integrity and authenticity of CCSDS command packets using an HMAC (Hash-Based Message Authentication Code).

It filters out unauthorized or tampered command packets before they are routed by the FprimeRouter.

TO DO
spacePacketDeframer.dataOut -> Authenticate.dataIn
Authenticate.dataOut -> fprimeRouter.dataIn
fprimeRouter.dataReturnOut -> Authenticate.dataReturnIn
Authenticate.dataReturnOut -> spacePacketDeframer.dataReturnIn


The goal here is to Forward only authorized packets via dataOut to fprimeRouter
forward unauthroized packets to dataReturnOut back to commsBufferManager which is the initial component in this chain. SHould we thinking about managing what we do with the invalid commands in commsBufferManager?


Port specifically for radio amateur, they get their own components
Radio amateurs

you could reserve a spreading factor (like 10 or smth for commands)

Questions
- do we want events for unexecuted commands?
- How do we wan this authentication within the packet

Hash within the actual packets

the smallest SHA hash is SHA-256

4 initial with radio callsign, 2 6b headers, at least one idle and then 8 that just go to lora

256-12=244 #2 headers of 6 bytes
244-4= 240 # 4 beginning where we can put callsign
240-1=29 # the one idle at least
239-6=233 #lora packs

233 is the FW_COM_BUFFER_MAX_SIZE, should we say the first 8 bits there is the hash, and the rest if the command,
233-8=225 which would be enough for the message itself

need to change the payload

| 0-7 bytes hash | 8-232 bytes FPrime Command Packet |

If valid:

Remove HMAC from the buffer (adjust data pointer + size)

Pass the clean buffer to FprimeRouter

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|AUTH001|The component shall only apply HMAC authentication to packets where the FrameContext.apid matches a configured list of command APIDs.|Inspection|
|AUTH002|The component shall forward radio amateur commands FrameContext.apid matches a configured list of command APIDs.|Inspection|
|AUTH003|If the packet's APID is not in the command list, the component shall forward the buffer unchanged to dataOut | Inspection\
|AUTH004|For applicable packets, the component shall extract the first N bytes of the buffer as the HMAC|---|
|AUTH006|The component shall compute the expected HMAC over the remaining payload and compare computed and received HMACs|---|
|AUTH006|If the HMAC is valid, the component shall emit an event and adjust the pointer in the buffer and pass the payload to dataout|---|
|AUTH007|If the HMAC is invalid, the component shall emit an event and send the invalid command back out the dataReturnOut port|---|
|AUTH008| The HMAC autenticifation will be computed with a secret key shared with the ground station and the message with the counter that is also attached to the message |---|
|AUTH009| The authenticate component will be able to send its current counter value to the ground station |---|



## Port Descriptions
| Name | Description |
|------|------|
|dataIn|---|
|dataOut|---|
|dataReturnOut|--|

## Events
| Name | Description |
|---|---|
|InvalidHash|Emits the opcode and has of an invalid component|
|ValidHashEmits the opcode and has of an valid component|


## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |
