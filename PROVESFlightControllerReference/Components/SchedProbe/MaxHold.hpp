// Research instrumentation for #601. Wall-clock max-hold around a handler body.
#ifndef Components_SchedProbe_MaxHold_HPP
#define Components_SchedProbe_MaxHold_HPP

#include <zephyr/kernel.h>
#include <Fw/FPrimeBasicTypes.hpp>

namespace Components {

struct MaxHold {
    const char* name;
    volatile U32 max_us;
    volatile U32 count;
};

class ScopedMaxHold {
  public:
    explicit ScopedMaxHold(MaxHold& hold) : m_hold(hold), m_t0(k_cycle_get_32()) {}
    ~ScopedMaxHold() {
        const U32 cycles = k_cycle_get_32() - this->m_t0;
        const U32 us = cycles / (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000000U);
        if (us > this->m_hold.max_us) {
            this->m_hold.max_us = us;
        }
        this->m_hold.count++;
    }

  private:
    MaxHold& m_hold;
    const U32 m_t0;
};

extern MaxHold g_modeManagerRun;
extern MaxHold g_modeManagerForceSafe;
extern MaxHold g_modeManagerCmd;

}  // namespace Components

#endif
