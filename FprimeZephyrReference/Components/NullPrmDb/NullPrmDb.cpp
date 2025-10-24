// ======================================================================
// \title  NullPrmDb.cpp
// \author starchmd
// \brief  cpp file for NullPrmDb component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/NullPrmDb/NullPrmDb.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

NullPrmDb ::NullPrmDb(const char* const compName) : NullPrmDbComponentBase(compName) {}

NullPrmDb ::~NullPrmDb() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::ParamValid NullPrmDb ::getPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    return Fw::ParamValid::INVALID;
}

void NullPrmDb ::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {}

}  // namespace Components
