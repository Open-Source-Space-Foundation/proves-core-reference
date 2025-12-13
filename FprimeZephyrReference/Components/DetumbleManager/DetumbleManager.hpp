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
    // ----------------------------------------------------------------------
    //  Private helper types
    // ----------------------------------------------------------------------

    //! Structure to hold magnetorquer coil parameters
    struct magnetorquerCoil {
        enum Shape { RECTANGULAR, CIRCULAR } shape;

        bool enabled;
        F64 maxCurrent;
        F64 numTurns;
        F64 voltage;
        F64 resistance;

        // Dimensions
        F64 width;     // For Rectangular
        F64 length;    // For Rectangular
        F64 diameter;  // For Circular
    };

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

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Perform a single B-Dot control step.
    bool executeControlStep(std::string& reason);

    //! Compute the angular velocity magnitude in degrees per second.
    F64 getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel);

    //! Compute the coil area based on its shape and dimensions.
    F64 getCoilArea(const magnetorquerCoil& coil);

    //! Compute the maximum coil current based on its voltage and resistance.
    F64 getMaxCoilCurrent(const magnetorquerCoil& coil);

    //! Set the dipole moment by toggling the magnetorquers
    void setDipoleMoment(Drv::DipoleMoment dpMoment);

    //! Set the magnetorquers on or off based on the provided values
    void setMagnetorquers(bool val[5]);

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    //! Mathematical constant pi
    const double PI = 3.14159265358979323846;

    //! X+ Coil parameters
    magnetorquerCoil m_xPlusMagnetorquer;

    //! X- Coil parameters
    magnetorquerCoil m_xMinusMagnetorquer;

    //! Y+ Coil parameters
    magnetorquerCoil m_yPlusMagnetorquer;

    //! Y- Coil parameters
    magnetorquerCoil m_yMinusMagnetorquer;

    //! Z- Coil parameters
    magnetorquerCoil m_zMinusMagnetorquer;

    U32 m_lastCompleted = 0;
    bool m_detumbleRunning = true;
    int m_itrCount = 0;
    bool m_bDotRunning = false;
    U32 m_bDotStartTime = -1;
};

}  // namespace Components

#endif
