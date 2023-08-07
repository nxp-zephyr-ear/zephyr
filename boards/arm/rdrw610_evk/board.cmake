# Copyright 2022 NXP
# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=RW610" "--reset-after-load")

board_runner_args(linkserver  "--device=RW610:RDRW610")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/linkserver.board.cmake)
