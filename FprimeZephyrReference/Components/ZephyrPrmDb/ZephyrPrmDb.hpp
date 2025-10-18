// ======================================================================
// \title  ZephyrPrmDb.hpp
// \brief  hpp file for ZephyrPrmDb component implementation class
// ======================================================================

#ifndef Components_ZephyrPrmDb_HPP
#define Components_ZephyrPrmDb_HPP

#include "FprimeZephyrReference/Components/ZephyrPrmDb/ZephyrPrmDbComponentAc.hpp"

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

namespace Components {

class ZephyrPrmDb final : public ZephyrPrmDbComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ZephyrPrmDb object
    ZephyrPrmDb(const char* const compName  //!< The component name
    );

    //! Destroy ZephyrPrmDb object
    ~ZephyrPrmDb();

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
