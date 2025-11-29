// ======================================================================
// \title  ThermalManager.cpp
// \brief  cpp file for ThermalManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ThermalManager/ThermalManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ThermalManager::ThermalManager(const char* const compName) : ThermalManagerComponentBase(compName) {}

ThermalManager::~ThermalManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ThermalManager::run_handler(FwIndexType portNum, U32 context) {
    // Cube face sensors (5 sensors)
    Fw::Success condition;

    // If the TCA is not healthy, skip reading sensors
    if (this->tcaHealthGet_out(0) != Fw::Health::HEALTHY) {
        return;
    }

    if (this->muxChannel0HealthGet_out(0) == Fw::Health::HEALTHY &&
        this->face0LoadSwitchStateGet_out(0) == Fw::On::ON) {
        this->face0Init_out(0, condition);  // TODO(nateinaction): If init fails, try deinit?
        if (condition == Fw::Success::SUCCESS) {
            this->face0TempGet_out(0);
        }
    }

    if (this->muxChannel1HealthGet_out(0) == Fw::Health::HEALTHY &&
        this->face1LoadSwitchStateGet_out(0) == Fw::On::ON) {
        this->face1Init_out(0, condition);
        if (condition == Fw::Success::SUCCESS) {
            this->face1TempGet_out(0);
        }
    }

    if (this->muxChannel2HealthGet_out(0) == Fw::Health::HEALTHY &&
        this->face2LoadSwitchStateGet_out(0) == Fw::On::ON) {
        this->face2Init_out(0, condition);
        if (condition == Fw::Success::SUCCESS) {
            this->face2TempGet_out(0);
        }
    }

    if (this->muxChannel3HealthGet_out(0) == Fw::Health::HEALTHY &&
        this->face3LoadSwitchStateGet_out(0) == Fw::On::ON) {
        this->face3Init_out(0, condition);
        if (condition == Fw::Success::SUCCESS) {
            this->face3TempGet_out(0);
        }
    }

    if (this->muxChannel5HealthGet_out(0) == Fw::Health::HEALTHY &&
        this->face5LoadSwitchStateGet_out(0) == Fw::On::ON) {
        this->face5Init_out(0, condition);
        if (condition == Fw::Success::SUCCESS) {
            this->face5TempGet_out(0);
        }
    }

    // If mux channel 4 is not healthy, skip battery cell sensors
    if (this->muxChannel4HealthGet_out(0) != Fw::Health::HEALTHY) {
        return;
    }

    // Battery cell sensors (4 sensors)
    this->battCell1Init_out(0, condition);
    if (condition == Fw::Success::SUCCESS) {
        this->battCell1TempGet_out(0);
    }

    this->battCell2Init_out(0, condition);
    if (condition == Fw::Success::SUCCESS) {
        this->battCell2TempGet_out(0);
    }

    this->battCell3Init_out(0, condition);
    if (condition == Fw::Success::SUCCESS) {
        this->battCell3TempGet_out(0);
    }

    this->battCell4Init_out(0, condition);
    if (condition == Fw::Success::SUCCESS) {
        this->battCell4TempGet_out(0);
    }
}

}  // namespace Components
