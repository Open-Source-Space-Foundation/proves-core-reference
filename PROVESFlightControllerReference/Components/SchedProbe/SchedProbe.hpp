#ifndef Components_SchedProbe_HPP
#define Components_SchedProbe_HPP

#include "PROVESFlightControllerReference/Components/SchedProbe/SchedProbeComponentAc.hpp"
#include <zephyr/kernel.h>

namespace Components {

class SchedProbe final : public SchedProbeComponentBase {
  public:
    SchedProbe(const char* const compName);
    ~SchedProbe();

    //! Spawn the probe threads and the reporter thread. Call once after startTasks().
    void start();

  private:
    void REPORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void RESET_PROBES_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    static void reporterEntry(void* self, void*, void*);
    void report();
    void reportThread(k_tid_t thread);
    static void threadCb(const ::k_thread* thread, void* self);

    volatile bool m_reportRequested = false;
};

}  // namespace Components

#endif
