// ======================================================================
// \title  DetumbleManager.hpp
// \brief  hpp file for DetumbleManager component implementation class
// ======================================================================

#ifndef Components_DetumbleManager_HPP
#define Components_DetumbleManager_HPP

#include <cmath>
#include <string>

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManagerComponentAc.hpp"

namespace Components {

class DetumbleManager final : public DetumbleManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DetumbleManager object
    DetumbleManager(const char* const compName);

    //! Destroy DetumbleManager object
    ~DetumbleManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // Constants
    Drv::MagneticField EMPTY_MG_FIELD = Drv::MagneticField(0.0, 0.0, 0.0, -1);
    Drv::DipoleMoment EMPTY_DP_MOMENT = Drv::DipoleMoment(0.0, 0.0, 0.0);
    const double PI = 3.14159265358979323846;

    // Proves V3 Magnetorquer Information
    F64 COIL_VOLTAGE = 3.3;
    F64 COIL_NUM_TURNS_X_Y = 48;
    F64 COIL_LENGTH_X_Y = 0.053;
    F64 COIL_WIDTH_X_Y = 0.045;
    F64 COIL_AREA_X_Y = this->COIL_LENGTH_X_Y * this->COIL_WIDTH_X_Y;
    F64 COIL_RESISTANCE_X_Y = 57.2;
    F64 COIL_MAX_CURRENT_X_Y = this->COIL_VOLTAGE / this->COIL_RESISTANCE_X_Y;
    I64 COIL_NUM_TURNS_Z = 153;
    F64 COIL_DIAMETER_Z = 0.05755;
    F64 COIL_AREA_Z = this->PI * pow(this->COIL_DIAMETER_Z / 2, 2.0);
    F64 COIL_RESISTANCE_Z = 248.8;
    F64 COIL_MAX_CURRENT_Z = this->COIL_VOLTAGE / this->COIL_RESISTANCE_Z;

    // Variables
    Drv::MagneticField prevMgField = Drv::MagneticField(0.0, 0.0, 0.0, -1);
    int iterations = 0;
    U32 startTime = 0;
    U32 lastCompleted = 0;
    bool detumbleRunning = true;
    int m_itrCount = 0;

    bool bDotRunning = false;
    U32 bDotStartTime = -1;

    // Functions
    bool executeControlStep(std::string& reason);
    void setDipoleMoment(Drv::DipoleMoment dpMoment);
    F64 getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel);
    Components::InputArray generateInputArray(bool val[5]);
};

}  // namespace Components

#endif
