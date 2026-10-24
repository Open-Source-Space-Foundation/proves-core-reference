// ======================================================================
// \title  PowerIntegrator.hpp
// \brief  hpp file for PowerIntegrator class
// ======================================================================

#pragma once

namespace Components {

//! Integrates consumed and generated power samples into energy totals (mWh)
//!
//! Both totals are integrated over the same time step, measured once per sample, so that each counter is
//! credited with the full interval between samples.
class PowerIntegrator {
  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PowerIntegrator object
    PowerIntegrator();

    //! Destroy PowerIntegrator object
    ~PowerIntegrator();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Integrate one pair of power samples taken at the given time
    //!
    //! The first call only establishes the time base. Power samples outside 0-1000 W are ignored, and time steps
    //! that are not positive or are 10 s or longer (e.g. time jumps) are not integrated.
    void update(double now_s,      //!< The time of the samples in seconds
                double consumedW,  //!< The consumed power sample in watts
                double generatedW  //!< The generated power sample in watts
    );

    //! Reset the accumulated consumed energy to 0 mWh
    void resetConsumption();

    //! Reset the accumulated generated energy to 0 mWh
    void resetGeneration();

    //! Get the accumulated consumed energy in mWh
    float getConsumption_mWh() const;

    //! Get the accumulated generated energy in mWh
    float getGeneration_mWh() const;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Add the energy of a power sample over a time step to a total
    static void accumulate(double& total_mWh,  //!< The total to add to in mWh
                           double powerW,      //!< The power sample in watts
                           double dt_s         //!< The time step in seconds
    );

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    double m_consumption_mWh;   //!< Accumulated consumed energy in mWh
    double m_generation_mWh;    //!< Accumulated generated energy in mWh
    double m_lastUpdateTime_s;  //!< Time of the last sample in seconds
    bool m_initialized;         //!< Whether the time base has been established
};

}  // namespace Components
