// ======================================================================
// \title  ImuManager.hpp
// \brief  hpp file for ImuManager component implementation class
// ======================================================================

#ifndef Components_ImuManager_HPP
#define Components_ImuManager_HPP

#include "FprimeZephyrReference/Components/ImuManager/ImuManagerComponentAc.hpp"

namespace Components {

class ImuManager final : public ImuManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuManager object
    ImuManager(const char* const compName);

    //! Destroy ImuManager object
    ~ImuManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for accelerationGet
    //!
    //! Port to read the current acceleration in m/s^2.
    Drv::Acceleration accelerationGet_handler(FwIndexType portNum,  //!< The port number
                                              Fw::Success& condition) override;

    //! Handler implementation for angularVelocityGet
    //!
    //! Port to read the current angular velocity in rad/s.
    Drv::AngularVelocity angularVelocityGet_handler(FwIndexType portNum,  //!< The port number
                                                    Fw::Success& condition) override;

    //! Handler implementation for magneticFieldGet
    //!
    //! Port to read the current magnetic field in gauss.
    Drv::MagneticField magneticFieldGet_handler(FwIndexType portNum,  //!< The port number
                                                Fw::Success& condition) override;

    //! Handler implementation for run
    //!
    //! Port to trigger periodic data fetching and telemetry updating
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Apply axis orientation parameter to sensor readings
    void applyAxisOrientation(double* x, double* y, double* z);
};

}  // namespace Components

#endif
