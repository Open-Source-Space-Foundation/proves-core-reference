# SPDX-License-Identifier: Apache-2.0

# Add out-of-tree driver sources to the app target
if(CONFIG_SX1280)
    target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../drivers/sx1280/sx1280.c)
endif()
