// ======================================================================
// \title  TickDivider.hpp
// \brief  rate group tick divider used by ComDelay
// ======================================================================

#pragma once

#include <cstdint>

namespace Components {

class TickDivider {
  public:
    using Count = uint16_t;

    //! Count one rate group tick. Returns true on the first tick and then once every divider + 1 ticks.
    bool tick(Count divider) {
        const bool release = (m_count == 0);
        m_count = (m_count >= divider) ? 0 : static_cast<Count>(m_count + 1);
        return release;
    }

  private:
    Count m_count = 0;
};

}  // namespace Components
