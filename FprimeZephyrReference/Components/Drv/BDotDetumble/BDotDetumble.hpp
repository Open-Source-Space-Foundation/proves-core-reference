// ======================================================================
// \title  BDotDetumble.hpp
// \author aychar
// \brief  hpp file for BDotDetumble component implementation class
// ======================================================================

#ifndef Drv_BDotDetumble_HPP
#define Drv_BDotDetumble_HPP

#include <array>

#include "FprimeZephyrReference/Components/Drv/BDotDetumble/BDotDetumbleComponentAc.hpp"
namespace Drv {

class BDotDetumble final : public BDotDetumbleComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BDotDetumble object
    BDotDetumble(const char* const compName  //!< The component name
    );

    //! Destroy BDotDetumble object
    ~BDotDetumble();

  public:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------
    // Get the current dipole moment
    Drv::DipoleMoment dipoleMomentGet_handler(const FwIndexType portNum,
                                              const Drv::MagneticField& currMagField,
                                              const Drv::MagneticField& prevMagField) override;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------
    // Get magnitude
    F64 getMagnitude(Drv::MagneticField magField);

    // Get the time derivative of the magnetic field
    std::array<F64, 3> dB_dt(Drv::MagneticField currMagField, Drv::MagneticField prevMagField);

    // Get the time of the magnetic field reading
    Fw::Time magneticFieldReadingTime(const Drv::MagneticField magField);

    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------
  private:
    F64 m_gain = 1.0;  //!< Gain for B-Dot controller
};
}  // namespace Drv
#endif
