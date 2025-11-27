module Components {
    @ Template Component for Payload Data Handler
    passive component PayloadTemplate {

        # Commands
        @ Send command to your payload over uart
        sync command SEND_COMMAND(cmd: string) # cmd: command bytecode

        # Events
        @ Error for if the command fails to send
        event CommandError(cmd: string) severity warning high format "Failed to send {} command"

        @ Log if the command got sent properly
        event CommandSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        # Ports
        @ Sends command to PayloadCom to be forwarded over UART
        output port commandOut: Drv.ByteStreamData

        @ Receives data from PayloadCom, handler to be implemented for your specific payload
        sync input port dataIn: Drv.ByteStreamData
        

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        # @ Example parameter
        # param PARAMETER_NAME: U32

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

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}