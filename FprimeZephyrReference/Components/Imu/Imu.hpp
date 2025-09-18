// ======================================================================
// \title  Imu.hpp
// \brief  hpp file for Imu component implementation class
// ======================================================================

#ifndef Components_Imu_HPP
#define Components_Imu_HPP

#include "FprimeZephyrReference/Components/Imu/ImuComponentAc.hpp"

namespace Components {

class Imu final : public ImuComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Imu object
    Imu(const char* const compName);

    //! Destroy Imu object
    ~Imu();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;
};

}  // namespace Components

#endif
