"""
wdk.py:

This module provides well defined keys for integration tests.
"""

# Deployment Names
reference_deployment = "ReferenceDeployment"

# Component Names
antenna_deployer = f"{reference_deployment}.antennaDeployer"
burnwire = f"{reference_deployment}.burnwire"
lsm6dso_manager = f"{reference_deployment}.lsm6dsoManager"
lis2mdl_manager = f"{reference_deployment}.lis2mdlManager"
rtc_manager = f"{reference_deployment}.rtcManager"
watchdog = f"{reference_deployment}.watchdog"
