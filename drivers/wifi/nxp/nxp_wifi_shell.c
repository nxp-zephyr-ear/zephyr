/**
 * Copyright (c) 2020 Linaro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "nxp_wifi_drv.h"

static struct nxp_wifi_dev *nxp_wifi;

void nxp_wifi_shell_register(struct nxp_wifi_dev *dev)
{
	/* only one instance supported */
	if (nxp_wifi) {
		return;
	}

	nxp_wifi = dev;
}

static int nxp_wifi_shell_cmd(const struct shell *sh, size_t argc, char **argv)
{
	int i;
	size_t len = 0;

	if (nxp_wifi == NULL) {
		shell_print(sh, "no nxp_wifi device registered");
		return -ENOEXEC;
	}

	if (argc < 2) {
		shell_help(sh);
		return -ENOEXEC;
	}

	nxp_wifi_lock(nxp_wifi);

	memset(nxp_wifi->buf, 0, sizeof(nxp_wifi->buf));
	for (i = 1; i < argc; i++) {
		size_t argv_len = strlen(argv[i]);

		if ((len + argv_len) >= sizeof(nxp_wifi->buf) - 1) {
			break;
		}

		memcpy(nxp_wifi->buf + len, argv[i], argv_len);
		len += argv_len;
		if (argc > 2) {
			memcpy(nxp_wifi->buf + len, " ", 1);
			len += 1;
		}
	}
	nxp_wifi->buf[len] = '\0';

	nxp_wifi_cmd(nxp_wifi, nxp_wifi->buf);

	nxp_wifi_unlock(nxp_wifi);

	return 0;
}

SHELL_CMD_ARG_REGISTER(nxp_wifi, NULL, "NXP Wi-Fi commands (Use help)", nxp_wifi_shell_cmd, 2,
		       CONFIG_SHELL_ARGC_MAX);
