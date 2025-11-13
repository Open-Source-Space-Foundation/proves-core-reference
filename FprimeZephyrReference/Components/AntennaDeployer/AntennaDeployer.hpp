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
    void distanceIn_handler(FwIndexType portNum, F32 distance, bool valid) override;

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
    enum class DeploymentState : U8 { IDLE = 0, QUIET_WAIT, BURNING, RETRY_WAIT };

    void startNextAttempt();
    void handleQuietWaitTick();
    void handleBurningTick();
    void handleRetryWaitTick();
    void finishDeployment(Components::DeployResult result);
    void resetDeploymentState();
    bool isDistanceWithinValidRange(F32 distance);
    bool isDistanceDeployed(F32 distance);
    void ensureBurnwireStopped();
    void logBurnSignalCount();
    bool readDeploymentState();
    void writeDeploymentState();

    DeploymentState m_state = DeploymentState::IDLE;
    U32 m_currentAttempt = 0;
    U32 m_ticksInState = 0;
    U32 m_totalAttempts = 0;
    bool m_successDetected = false;
    bool m_lastDistanceValid = false;
    F32 m_lastDistance = 0.0F;
    U32 m_burnTicksThisAttempt = 0;
};

}  // namespace Components

#endif
