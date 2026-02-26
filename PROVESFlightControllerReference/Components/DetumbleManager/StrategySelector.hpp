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

    //! Detumble strategies
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

    //! Determine detumble strategy based on angular velocity magnitude
    Strategy fromAngularVelocityMagnitude(
        double angular_velocity_magnitude_deg_sec  //!< Angular velocity magnitude in deg/s
    );

    //! Configure detumble strategy thresholds
    void configure(double bdot_max_threshold,        //!< B-Dot maximum rotational threshold in deg/s
                   double deadband_upper_threshold,  //!< Upper deadband rotational threshold in deg/s
                   double deadband_lower_threshold   //!< Lower deadband rotational threshold in deg/s
    );

  private:
    // ----------------------------------------------------------------------
    //  Private members variables
    // ----------------------------------------------------------------------

    double m_bdot_max_threshold;        //!< B-Dot maximum rotational threshold in deg/s
    double m_deadband_lower_threshold;  //!< Lower deadband threshold in deg/s
    double m_deadband_upper_threshold;  //!< Upper deadband threshold

    double m_rotation_target;  //!< Target angular velocity to achieve in deg/s
};

}  // namespace Components
