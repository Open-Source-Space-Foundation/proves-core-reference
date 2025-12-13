####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
# UT_SOURCE_FILES: list of source files for unit tests
#
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/AuthenticationRouter.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/AuthenticationRouter.cpp"
)
register_fprime_module()


#### UTS ####
set(UT_SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/AuthenticationRouter.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/AuthenticationRouter.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/AuthenticationRouter.cpp"
)
set(UT_AUTO_HELPERS ON)


register_fprime_ut()
