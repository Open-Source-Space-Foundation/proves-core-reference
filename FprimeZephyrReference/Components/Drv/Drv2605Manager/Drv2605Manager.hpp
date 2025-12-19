// ======================================================================
// \title  Drv2605Manager.hpp
// \author aychar
// \brief  hpp file for Drv2605Manager component implementation class
// ======================================================================

#ifndef Drv_Drv2605Manager_HPP
#define Drv_Drv2605Manager_HPP

#include "FprimeZephyrReference/Components/Drv/Drv2605Manager/Drv2605ManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/drivers/sensor.h>

namespace Drv {

class Drv2605Manager final : public Drv2605ManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Drv2605Manager object
    Drv2605Manager(const char* const compName  //!< The component name
    );

    //! Destroy Drv2605Manager object
    ~Drv2605Manager();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the DRV2605 device
    void configure(const struct device* tca, const struct device* mux, const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for loadSwitchStateChanged
    //!
    //! Port to initialize and deinitialize the device on load switch state change
    Fw::Success loadSwitchStateChanged_handler(FwIndexType portNum,  //!< The port number
                                               const Fw::On& state) override;
    //! Handler implementation for run
    //!
    //! Port to be called by rategroup to trigger magnetorquer when continuous mode is enabled
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for start
    //!
    //! Port to start the magnetorquer
    Fw::Success start_handler(FwIndexType portNum,  //!< The port number
                              U32 duration_us,      //!< Duration in microseconds to trigger the magnetorquer
                              I8 amps               //!< Amplitude value between -127 and 127
                              ) override;

    //! Handler implementation for stop
    //!
    //! Port to stop the magnetorquer
    void stop_handler(FwIndexType portNum  //!< The port number
                      ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TRIGGER
    //!
    //! Command to trigger the magnetorquer
    void TRIGGER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                            U32 cmdSeq            //!< The command sequence number
                            ) override;

    //! Handler implementation for command START_CONTINUOUS_MODE
    //!
    //! Command to start continuous mode
    void START_CONTINUOUS_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                          U32 cmdSeq            //!< The command sequence number
                                          ) override;

    //! Handler implementation for command STOP_CONTINUOUS_MODE
    //!
    //! Command to stop continuous mode
    void STOP_CONTINUOUS_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                         U32 cmdSeq            //!< The command sequence number
                                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Initialize the TMP112 device
    Fw::Success initializeDevice();

    //! Deinitialize the TMP112 device
    Fw::Success deinitializeDevice();

    //! Check if the TMP112 device is initialized
    bool isDeviceInitialized();

    //! Check if the load switch is ready (on and timeout passed)
    bool loadSwitchReady();

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized TMP112 sensor
    const struct device* m_dev;

    //! Zephyr device for the TCA
    const struct device* m_tca;

    //! Zephyr device for the mux
    const struct device* m_mux;

    //! Load switch state
    Fw::On m_load_switch_state = Fw::On::OFF;

    //! Load switch on timeout
    //! Time when we can consider the load switch to be fully on (giving time for power to normalize)
    Fw::Time m_load_switch_on_timeout;

    //! Load switch check
    //! Available to disable if the component is not powered by a load switch
    bool m_load_switch_check = true;

    //! Continuous mode
    //! If true, the magnetorquer will be triggered on every run port call
    bool m_continuous_mode = false;
    bool m_has_initialized = false;
    int m_count = 0;
};

}  // namespace Drv

#endif
