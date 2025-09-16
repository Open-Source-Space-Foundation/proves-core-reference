module Components {
    # @ Extern type to define zepyhr sensor_value
    # struct SensorValue {
    #     val1: I32
    #     val2: I32
    # }
    @ Acceleration Reading in m/s^2
    struct Acceleration {
        x: F64
        y: F64
        z: F64
    }

    @ Angular velocity reading in rad/s
    struct AngularVelocity {
        x: F64
        y: F64
        z: F64
    }

    port AccelerationRead() -> Acceleration
    port AngularVelocityRead() -> AngularVelocity
    port TemperatureRead() -> F64
    
    @ Initialize and control operation of the lms6dso device
    passive component lms6dsoDriver {

        @ Port for synchronously retrieving data
        sync input port getAcceleration: AccelerationRead
        sync input port getAngularVelocity: AngularVelocityRead
        sync input port getTemperature: TemperatureRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        
        

    }
}