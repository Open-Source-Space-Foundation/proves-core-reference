module Components {
    enum DeployResult {
        DEPLOY_RESULT_SUCCESS @< Deployment completed successfully
        DEPLOY_RESULT_ABORT   @< Deployment aborted via command
        DEPLOY_RESULT_FAILED  @< Deployment failed after exhausting retries
    }
}

module Components {
    port DistanceUpdate(
        distance: F32 @< Latest measured distance in centimeters
        valid: bool @< Flag indicating the distance value is considered valid
    )
}

module Components {
    @ Component that deploys the antenna, activates the burnwire, checks the distance sensor
    passive component AntennaDeployer {
        ######################################################################
        # Commands
        ######################################################################
        @ DEPLOY starts the deployment procedure
        sync command DEPLOY()

        @ DEPLOY_STOP stops the deployment procedure
        sync command DEPLOY_STOP()

        @ RESET_DEPLOYMENT_STATE resets the deployment state flag for ground testing
        sync command RESET_DEPLOYMENT_STATE()

        @ SET_DEPLOYMENT_STATE forces the persistent deployment state for ground testing
        sync command SET_DEPLOYMENT_STATE(
            deployed: bool @< True to mark as deployed, false to clear the flag
        )

        ######################################################################
        # Telemetry
        ######################################################################
        @ Counts the number of deployment attempts
        telemetry DeployAttemptCount: U32

        @ Tracks the last observed distance reading
        telemetry LastDistance: F32 format "{.2f}cm"

        ######################################################################
        # Events
        ######################################################################
        @ Emitted at the start of each deployment attempt
        event DeployAttempt(
            attempt: U32 @< The attempt number (starting at 1)
        ) severity activity high \
          format "Antenna deployment attempt {} started"

        @ Emitted when the antenna deployment is considered successful
        event DeploySuccess(
            attempt: U32 @< Attempt number that succeeded
        ) severity activity high \
          format "Antenna deployment succeeded on attempt {}"

        @ Emitted when the deployment procedure finishes
        event DeployFinish(
            result: Components.DeployResult @< Final deployment outcome
            attempts: U32 @< Number of attempts completed
        ) severity activity high \
          format "Antenna deployment finished with result {} after {} attempts"

        @ Emitted when a distance reading is ignored because it is invalid
        event InvalidDistanceMeasurement(
            distance: F32 @< Distance provided
        ) severity warning low \
          format "Ignoring invalid antenna distance measurement: {.2f} cm"

        @ Emitted when the quiet wait period expires and deployment attempt begins
        event QuietTimeExpired(
            elapsedTime: U32 @< Time elapsed in seconds during quiet wait
        ) severity activity high \
          format "Quiet time expired after {} seconds, starting deployment attempt"

        @ Reports how many scheduler ticks the burn signal was held active for the latest attempt
        event AntennaBurnSignalCount(
            ticks: U32 @< Number of scheduler ticks spent in the burn state
        ) severity activity low \
          format "Burn signal active for {} scheduler ticks"

        @ Emitted when deployment is skipped because antenna was already deployed
        event DeploymentAlreadyComplete() \
          severity activity high \
          format "Antenna deployment skipped - antenna already deployed"

        ######################################################################
        # Ports
        ######################################################################
        @ Port receiving calls from the rate group
        sync input port schedIn: Svc.Sched

        @ Port receiving latest distance measurements
        sync input port distanceIn: Components.DistanceUpdate

        @ Port signaling the burnwire component to start heating
        output port burnStart: Fw.Signal

        @ Port signaling the burnwire component to stop heating
        output port burnStop: Fw.Signal

        ######################################################################
        # Parameters
        ######################################################################
        @ Quiet time (seconds) to wait after DEPLOY before the first burn attempt
        param QUIET_TIME_SEC: U32 default 120

        @ Delay (seconds) between burn attempts
        param RETRY_DELAY_SEC: U32 default 30

        @ Maximum number of burn attempts before giving up
        param MAX_DEPLOY_ATTEMPTS: U32 default 3

        @ Duration (seconds) for which to hold each burn attempt before issuing STOP
        param BURN_DURATION_SEC: U32 default 8

        @ Distance threshold (cm) under which the antenna is considered deployed
        param DEPLOYED_THRESHOLD_CM: F32 default 5.0

        @ Distance readings above this value (cm) are considered invalid
        param INVALID_THRESHOLD_TOP_CM: F32 default 500.0

        @ Distance readings below this value (cm) are considered invalid
        param INVALID_THRESHOLD_BOTTOM_CM: F32 default 0.1

        @ File path for persistent deployment state (file exists = deployed)
        param DEPLOYED_STATE_FILE: string default "/antenna_deployed.bin"

        ########################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, Parameters
        ########################################################################
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

        @ Port to set the value of a parameter
        param set port prmSetOut
    }
}
