// ======================================================================
// \title  AntennaDeployer.hpp
// \author aldjia
// \brief  hpp file for AntennaDeployer component implementation class
// ======================================================================

#ifndef Components_AntennaDeployer_HPP
#define Components_AntennaDeployer_HPP

#include "PROVESFlightControllerReference/Components/AntennaDeployer/AntennaDeployerComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/AntennaDeployer/DeploymentSequencer.hpp"

namespace Components {

class AntennaDeployer final : public AntennaDeployerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AntennaDeployer object
    AntennaDeployer(const char* const compName  //!< The component name
    );

    //! Destroy AntennaDeployer object
    ~AntennaDeployer();

    //! Initialize the component
    void init(FwEnumStoreType instance = 0  //!< Instance ID
    );

  private:
    // ----------------------------------------------------------------------
    // Handler implementations
    // ----------------------------------------------------------------------
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // Command handlers
    // ----------------------------------------------------------------------
    void DEPLOY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void DEPLOY_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void RESET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void SET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool deployed) override;

    // ----------------------------------------------------------------------
    // Internal helpers
    // ----------------------------------------------------------------------
    //! Carry out the effects the sequencer asked for, in order
    void applyStep(const DeploymentSequencer::Step& step);
    void ensureBurnwireStopped();
    bool readDeploymentState();
    void writeDeploymentState(bool deployed);

    DeploymentSequencer m_sequencer;
};

}  // namespace Components

#endif
