####
# FileExpander: rebuilds UPZ-compressed uplinked files (tools/upz/upz.py).
# UpzExpander.cpp has no F Prime dependency; tools/upz/test_upz.py tests it on the host.
####
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FileExpander.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FileExpander.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/UpzExpander.cpp"
    DEPENDS
        FprimeExtras_Update_Delta
)
