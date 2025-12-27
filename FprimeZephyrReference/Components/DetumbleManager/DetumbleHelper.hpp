// ======================================================================
// \title  DetumbleHelper.hpp
// \brief  hpp file for DetumbleHelper component implementation class
// ======================================================================

#pragma once

namespace Components {

class DetumbleHelper {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DetumbleHelper object
    DetumbleHelper();

    //! Destroy DetumbleHelper object
    ~DetumbleHelper();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Compute the angular velocity magnitude in degrees per second.
    //! Formula: |ω| = sqrt(ωx² + ωy² + ωz²)
    //!
    //! ωx, ωy, ωz are the angular velocity components in rad/s
    //! Returns magnitude in deg/s
    double getAngularVelocityMagnitude(const Drv::AngularVelocity& angular_velocity);
};

}  // namespace Components
