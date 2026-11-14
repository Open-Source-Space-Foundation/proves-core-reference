####
# Gnc/AstroLib
#
# F Prime independent astrodynamics kernel. Registered as an F Prime
# module so it can be a dependency for F Prime components.
####
register_fprime_module(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/AstroLib.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/AstroLib.hpp"
)
