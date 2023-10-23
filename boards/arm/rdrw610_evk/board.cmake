# Copyright 2022 NXP
# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=RW610" "--reset-after-load")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
