// ======================================================================
// \title  Persistence.hpp
// \brief  Pure bookkeeping for the persist-on-change sequence number policy
// ======================================================================

#ifndef Components_TcSecurityDeframer_Persistence
#define Components_TcSecurityDeframer_Persistence

#include <cstdint>

namespace Components {
namespace SequencePersistence {

//! Whether the on-disk value must be rewritten on this tick
inline bool needsWrite(uint32_t persisted,  //!< Last value known to be on disk
                       uint32_t current     //!< Current in-memory counter
) {
    return persisted != current;
}

//! The on-disk belief after a write attempt: a failed write leaves the previous belief standing so the
//! next tick retries
inline uint32_t afterWrite(uint32_t persisted,  //!< Last value known to be on disk before the attempt
                           uint32_t written,    //!< Value that was written
                           bool writeOk         //!< Whether the write succeeded
) {
    return writeOk ? written : persisted;
}

}  // namespace SequencePersistence
}  // namespace Components

#endif
