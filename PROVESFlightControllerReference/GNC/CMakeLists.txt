####
# Gnc
#
# Guidance, Navigation and Control. Registered top-to-bottom in dependency order.
#
#   AstroLib     Standalone C++ astrodynamics kernel, No F Prime.
#                Registered first because GncConvert.hpp converts its types.
#
#   Types        Shared FPP types plus conversions between AstroLib and FPP.
#                Must be registered before any consumer -- including
#                sensor drivers elsewhere in the project that emit
#                Gnc.VectorSample.
#
#   Environment  Models of the world: where the spacecraft is, where
#                the Sun is, what the field is.
#
#   Adcs         Attitude determination and control.
#
####
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/AstroLib/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Types/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Environment/")
