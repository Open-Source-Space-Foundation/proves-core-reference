// ======================================================================
// \title  ZephyrPrmDb.cpp
// \brief  cpp file for ZephyrPrmDb component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ZephyrPrmDb/ZephyrPrmDb.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace Components {

// struct settings_handler prmDb_handler = {.name = "PrmDb"};

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ZephyrPrmDb ::ZephyrPrmDb(const char* const compName) : ZephyrPrmDbComponentBase(compName) {
    int rc = settings_subsys_init();
    if (rc != 0) {
        printk("Failed to initialize settings subsystem: %d\n", rc);
    }

    // rc = settings_register(&prmDb_handler);
    // if (rc != 0) {
    //     // log or event the error
    // }
}

ZephyrPrmDb ::~ZephyrPrmDb() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::ParamValid ZephyrPrmDb ::getPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    char idStr[11];  // Max for 32-bit unsigned int is 10 digits + null terminator
    snprintf(idStr, sizeof(idStr), "%u", id);

    int val_size = settings_get_val_len(idStr);
    int rc = settings_load_one(idStr, &val, val_size);
    if (rc != 0) {
        // log or event the error
        printk("Failed to load parameter ID %u: %d\n", id, rc);
        return Fw::ParamValid::INVALID;
    }
    return Fw::ParamValid::VALID;
}

void ZephyrPrmDb ::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    char idStr[11];  // Max for 32-bit unsigned int is 10 digits + null terminator
    snprintf(idStr, sizeof(idStr), "%u", id);

    int rc = settings_save_one(idStr, &val, sizeof(val));
    if (rc != 0) {
        // log or event the error
        printk("Failed to save parameter ID %u: %d\n", id, rc);
    }
}
}  // namespace Components
