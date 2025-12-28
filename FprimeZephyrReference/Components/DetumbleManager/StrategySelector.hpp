// ======================================================================
// \title  StrategySelector.hpp
// \brief  hpp file for StrategySelector implementation class
// ======================================================================

#pragma once

#include <array>

namespace Components {

class StrategySelector {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    enum Strategy {
        IDLE = 0,        //<! Do not detumble
        BDOT = 1,        //<! Use B-Dot detumbling
        HYSTERESIS = 2,  //<! Use hysteresis detumbling
    };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StrategySelector object
    StrategySelector();

    //! Destroy StrategySelector object
    ~StrategySelector();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Determine detumble strategy based on angular velocity
    Strategy fromAngularVelocity(std::array<double, 3> angular_velocity  //!< Angular velocity in rad/s
    );

    //! Configure detumble strategy thresholds
    void configure(double bdot_max_threshold,        //!< B-Dot maximum rotational threshold in deg/s
                   double deadband_upper_threshold,  //!< Upper deadband rotational threshold in deg/s
                   double deadband_lower_threshold   //!< Lower deadband rotational threshold in deg/s
    );

    //! Reset rotation target to lower deadband threshold
    void resetRotationTarget();

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Compute the angular velocity magnitude in degrees per second.
    //! Formula: |ω| = sqrt(ωx² + ωy² + ωz²)
    //!
    //! ωx, ωy, ωz are the angular velocity components in rad/s
    //! Returns magnitude in deg/s
    double getAngularVelocityMagnitude(std::array<double, 3> angular_velocity  //!< Angular velocity in rad/s
    );

  private:
    // ----------------------------------------------------------------------
    //  Private members variables
    // ----------------------------------------------------------------------

    const double PI = 3.14159265358979323846;  //!< Mathematical constant pi

    double m_bdot_max_threshold;        //!< B-Dot maximum rotational threshold in deg/s
    double m_deadband_lower_threshold;  //!< Lower deadband threshold in deg/s
    double m_deadband_upper_threshold;  //!< Upper deadband threshold

    double m_rotation_target;  //!< Target angular velocity to achieve in deg/s
};

}  // namespace Components
