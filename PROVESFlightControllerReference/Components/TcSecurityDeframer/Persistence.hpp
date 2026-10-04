// ======================================================================
// \title  Persistence.hpp
// \brief  Pure bookkeeping for the persist-on-change sequence number policy
// ======================================================================

#ifndef Components_TcSecurityDeframer_Persistence
#define Components_TcSecurityDeframer_Persistence

#include <cstdint>

namespace Components {
namespace SequencePersistence {

//! What the component believes the sequence number file holds
struct OnDisk {
    bool known;      //!< False after a failed write: the open truncates the file, so its content is unknown
    uint32_t value;  //!< Value on disk when known
};

//! Whether the file must be rewritten on this tick: always after a failed write, otherwise only when
//! the counter moved
inline bool needsWrite(const OnDisk& onDisk,  //!< Current belief about the file
                       uint32_t current       //!< Current in-memory counter
) {
    return (!onDisk.known) || (onDisk.value != current);
}

//! The belief after a write attempt
inline OnDisk afterWrite(uint32_t written,  //!< Value that was written
                         bool writeOk       //!< Whether the write succeeded
) {
    return OnDisk{writeOk, written};
}

}  // namespace SequencePersistence
}  // namespace Components

#endif
