// ======================================================================
// \title  NullPrmDb.hpp
// \author starchmd
// \brief  hpp file for NullPrmDb component implementation class
// ======================================================================

#ifndef Components_NullPrmDb_HPP
#define Components_NullPrmDb_HPP

#include "FprimeZephyrReference/Components/NullPrmDb/NullPrmDbComponentAc.hpp"

namespace Components {

class NullPrmDb final : public NullPrmDbComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct NullPrmDb object
    NullPrmDb(const char* const compName  //!< The component name
    );

    //! Destroy NullPrmDb object
    ~NullPrmDb();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for getPrm
    //!
    //! Port to get parameter values
    Fw::ParamValid getPrm_handler(FwIndexType portNum,  //!< The port number
                                  FwPrmIdType id,       //!< Parameter ID
                                  Fw::ParamBuffer& val  //!< Buffer containing serialized parameter value.
                                                        //!< Unmodified if param not found.
                                  ) override;

    //! Handler implementation for setPrm
    //!
    //! Port to update parameters
    void setPrm_handler(FwIndexType portNum,  //!< The port number
                        FwPrmIdType id,       //!< Parameter ID
                        Fw::ParamBuffer& val  //!< Buffer containing serialized parameter value
                        ) override;
};

}  // namespace Components

#endif
