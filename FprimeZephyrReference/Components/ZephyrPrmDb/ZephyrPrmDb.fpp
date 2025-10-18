module Components {
    @ Zephyr parameter database
    passive component ZephyrPrmDb {
        @ Port to get parameter values
        sync input port getPrm: Fw.PrmGet

        @ Port to update parameters
        sync input port setPrm: Fw.PrmSet
    }
}
