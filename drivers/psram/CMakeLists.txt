# Copyright (c) 2025
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
# Source always included; DT_INST_FOREACH_STATUS_OKAY yields no instances when node is disabled
zephyr_library_sources(aps1604m.c)
