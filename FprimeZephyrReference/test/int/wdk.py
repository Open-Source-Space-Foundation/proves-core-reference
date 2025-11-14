"""
wdk.py:

This module provides well defined keys for integration tests.
"""

# Deployment Names
reference_deployment = "ReferenceDeployment"

# Component Names
antenna_deployer = f"{reference_deployment}.antennaDeployer"
burnwire = f"{reference_deployment}.burnwire"
ina219SolManager = f"{reference_deployment}.ina219SolManager"
ina219SysManager = f"{reference_deployment}.ina219SysManager"
lis2mdl_manager = f"{reference_deployment}.lis2mdlManager"
loadswitch = f"{reference_deployment}.face0LoadSwitch"
lsm6dso_manager = f"{reference_deployment}.lsm6dsoManager"
powerMonitor = f"{reference_deployment}.powerMonitor"
rtc_manager = f"{reference_deployment}.rtcManager"
watchdog = f"{reference_deployment}.watchdog"

# Switch States
on = "ON"
off = "OFF"
