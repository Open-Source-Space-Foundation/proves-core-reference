// ======================================================================
// \title  BDotDetumble.hpp
// \author aychar
// \brief  hpp file for BDotDetumble component implementation class
// ======================================================================

#ifndef Drv_BDotDetumble_HPP
#define Drv_BDotDetumble_HPP

#include "FprimeZephyrReference/Components/Drv/BDotDetumble/BDotDetumbleComponentAc.hpp"
#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"

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

    // Get the current dipole moment
    Drv::DipoleMoment dipoleMomentGet_handler(const FwIndexType portNum,
                                              const Drv::MagneticField& currMagField,
                                              const Drv::MagneticField& prevMagField) override;

  private:
    F64 gain = 1.0;

    // Get magnitude
    F64 getMagnitude(Drv::MagneticField magField);

    // Get the time derivative of the magnetic field
    F64* dB_dt(Drv::MagneticField currMagField, Drv::MagneticField prevMagField);
};
}  // namespace Drv
#endif
