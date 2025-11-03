// ======================================================================
// \title  DetumbleManager.hpp
// \brief  hpp file for DetumbleManager component implementation class
// ======================================================================

#ifndef Components_DetumbleManager_HPP
#define Components_DetumbleManager_HPP

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

    // Variables
    Drv::MagneticField EMPTY_MG_FIELD = Drv::MagneticField{0.0, 0.0, 0.0};
    Drv::MagneticField prevMgField = Drv::MagneticField{0.0, 0.0, 0.0};

    // Functions
    bool executeControlStep();
};

}  // namespace Components

#endif
