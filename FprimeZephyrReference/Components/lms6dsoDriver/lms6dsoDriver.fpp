module Components {
    @ Initialize and control operation of the lms6dso device
    passive component lms6dsoDriver {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

       @ Acceleration reading in m/s^2
        struct Acceleration {
            x: F64
            y: F64
            z: F64
        }

        


        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for synchronously retrieving data
        sync input port getData: GetSensorData

    }
}