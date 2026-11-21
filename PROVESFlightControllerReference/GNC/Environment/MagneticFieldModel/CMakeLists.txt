####
# Gnc/MagneticFieldModel
#
# WmmModel.* holds the WMM evaluation/math, using the XYZGeomag library.
# MagneticFieldModel.* is the FPrime wrapper.
#
####
register_fprime_module(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/MagneticFieldModel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/WmmModel.cpp"
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/MagneticFieldModel.fpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/MagneticFieldModel.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/WmmModel.hpp"
    DEPENDS
        PROVESFlightControllerReference_GNC_Types
        PROVESFlightControllerReference_GNC_AstroLib
)

# Unit test for loading WMM coefficients from a .COF file
register_fprime_ut(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/WmmFileLoadTest.cpp"
    DEPENDS
        PROVESFlightControllerReference_GNC_Environment_MagneticFieldModel
)