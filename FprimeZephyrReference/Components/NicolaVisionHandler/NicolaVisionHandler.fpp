module Components {
    @ handles communications to and from the nicola vision over spi bus
    active component NicolaVisionHandler {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port
        async command TakePicture opcode 0

        @ Failed to take picture
        event TakePictureError() severity warning high format "Failed to take picture"

        @ Picture taken
        event PictureTaken() severity activity high format "Picture taken"

        @ Picture received
        event PictureReceived() severity activity high format "Picture received"

        @ Buffer allocation failed
        event BufferAllocationFailed() severity warning high format "Buffer allocation failed"

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
        output port out_port: Drv.ByteStreamSend 
        guarded input port in_port: Drv.ByteStreamData

        # buffer ports
        output port allocate: Fw.BufferGet
        output port deallocate: Fw.BufferSend
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