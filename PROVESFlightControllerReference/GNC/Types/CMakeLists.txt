####
# GncType
#
# Contains shared GNC data types and ports.
#
# Define all types that cross component boundaries here
# to keep other GNC components independent of each other.
#
####
register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/GncTypes.fpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/GncConvert.hpp"
    DEPENDS
        Fw_Time # Fw.Time is used by Gnc.VectorSample and Gnc.OrbitState
        PROVESFlightControllerReference_Gnc_AstroLib   # GncConvert.hpp converts Astro types
)
