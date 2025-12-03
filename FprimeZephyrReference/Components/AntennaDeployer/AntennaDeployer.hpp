// ======================================================================
// \title  AntennaDeployer.hpp
// \author aldjia
// \brief  hpp file for AntennaDeployer component implementation class
// ======================================================================

#ifndef Components_AntennaDeployer_HPP
#define Components_AntennaDeployer_HPP

#include "FprimeZephyrReference/Components/AntennaDeployer/AntennaDeployerComponentAc.hpp"

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
    enum class DeploymentState : U8 { IDLE = 0, BURNING, RETRY_WAIT };

    void startNextAttempt();
    void handleBurningTick();
    void handleRetryWaitTick();
    void finishDeployment(Components::DeployResult result);
    void resetDeploymentState();
    void ensureBurnwireStopped();
    void logBurnSignalCount();
    bool readDeploymentState();
    void writeDeploymentState(bool deployed);

    DeploymentState m_state = DeploymentState::IDLE;
    U32 m_currentAttempt = 0;
    U32 m_ticksInState = 0;
    U32 m_totalAttempts = 0;
    bool m_stopRequested = false;
    U32 m_burnTicksThisAttempt = 0;
};

}  // namespace Components

#endif
