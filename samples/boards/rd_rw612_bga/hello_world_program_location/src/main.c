/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

int main(void)
{
	printf("Hello World from %s! \nMain address: %p\n", CONFIG_BOARD, &main);
	return 0;
}
