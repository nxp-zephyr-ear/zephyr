/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_mipi_dbi_spi

#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mipi_dbi_spi, CONFIG_MIPI_DBI_LOG_LEVEL);

struct mipi_dbi_spi_config {
	/* SPI hardware used to send data */
	const struct device *spi_dev;
	/* Command/Data gpio */
	const struct gpio_dt_spec cmd_data;
	/* Reset GPIO */
	const struct gpio_dt_spec reset;
	/* Tracks if C/D is inverted from traditional polarity */
	bool cd_invert;
	/* Tracks if the reset signal is inverted from traditional polarity */
	bool rst_invert;
};

struct mipi_dbi_spi_data {
	/* Used for 3 wire mode */
	uint16_t spi_byte;
	struct k_spinlock lock;
};

static int mipi_dbi_spi_write_helper(const struct device *dev,
				     const struct mipi_dbi_config *dbi_config,
				     bool cmd_present, uint8_t cmd,
				     const uint8_t *data_buf, size_t len)
{
	const struct mipi_dbi_spi_config *config = dev->config;
	struct mipi_dbi_spi_data *data = dev->data;
	struct spi_buf buffer;
	struct spi_buf_set buf_set = {
		.buffers = &buffer,
		.count = 1,
	};
	int ret = 0;
	k_spinlock_key_t spinlock_key = k_spin_lock(&data->lock);

	if (dbi_config->mode == MIPI_DBI_MODE_SPI_3WIRE &&
	    IS_ENABLED(CONFIG_MIPI_DBI_SPI_3WIRE)) {
		struct spi_config tmp_cfg;
		/* We have to emulate 3 wire mode by packing the data/command
		 * bit into the upper bit of the SPI transfer.
		 * switch SPI to 9 bit mode, and write the transfer
		 */
		memcpy(&tmp_cfg, &dbi_config->config, sizeof(tmp_cfg));
		tmp_cfg.operation &= ~SPI_WORD_SIZE_MASK;
		tmp_cfg.operation |= SPI_WORD_SET(9);

		/* Send command */
		if (cmd_present) {
			if (config->cd_invert) {
				data->spi_byte = BIT(9) | cmd;
			} else {
				data->spi_byte = cmd;
			}
			buffer.buf = &data->spi_byte;
			buffer.len = 1;
			ret = spi_write(config->spi_dev, &tmp_cfg, &buf_set);
			if (ret < 0) {
				goto out;
			}
		}
		/* Write data, byte by byte */
		for (size_t i = 0; i < len; i++) {
			if (config->cd_invert) {
				data->spi_byte = data_buf[i];
			} else {
				data->spi_byte = BIT(9) | data_buf[i];
			}
			ret = spi_write(config->spi_dev, &tmp_cfg, &buf_set);
			if (ret < 0) {
				goto out;
			}
		}
	} else if (dbi_config->mode == MIPI_DBI_MODE_SPI_4WIRE) {
		/* 4 wire mode is much simpler. We just toggle the
		 * command/data GPIO to indicate if we are sending
		 * a command or data
		 */
		buffer.buf = &cmd;
		buffer.len = 1;

		if (cmd_present) {
			gpio_pin_set_dt(&config->cmd_data, config->cd_invert);
			ret = spi_write(config->spi_dev, &dbi_config->config,
					&buf_set);
			if (ret < 0) {
				goto out;
			}
		}

		if (len > 0) {
			buffer.buf = (void *)data_buf;
			buffer.len = len;

			gpio_pin_set_dt(&config->cmd_data, !config->cd_invert);
			ret = spi_write(config->spi_dev, &dbi_config->config,
					&buf_set);
			if (ret < 0) {
				goto out;
			}
		}
	} else {
		/* Otherwise, unsupported mode */
		ret = -ENOTSUP;
	}
out:
	k_spin_unlock(&data->lock, spinlock_key);
	return ret;
}


static int mipi_dbi_spi_command_write(const struct device *dev,
				      const struct mipi_dbi_config *dbi_config,
				      uint8_t cmd, const uint8_t *data_buf,
				      size_t len)
{
	return mipi_dbi_spi_write_helper(dev, dbi_config, true, cmd,
					 data_buf, len);
}

static int mipi_dbi_spi_write_transfer(const struct device *dev,
				       const struct mipi_dbi_config *dbi_config,
				       const uint8_t *data_buf, size_t len)
{
	return mipi_dbi_spi_write_helper(dev, dbi_config, false, 0x0,
					 data_buf, len);
}

static int mipi_dbi_spi_command_read(const struct device *dev,
				     const struct mipi_dbi_config *dbi_config,
				     uint8_t cmd, uint8_t *response,
				     size_t len)
{
	const struct mipi_dbi_spi_config *config = dev->config;
	struct mipi_dbi_spi_data *data = dev->data;
	struct spi_buf buffer;
	struct spi_buf_set buf_set = {
		.buffers = &buffer,
		.count = 1,
	};
	int ret;
	k_spinlock_key_t spinlock_key = k_spin_lock(&data->lock);

	if (dbi_config->mode == MIPI_DBI_MODE_SPI_3WIRE &&
	    IS_ENABLED(CONFIG_MIPI_DBI_SPI_3WIRE)) {
		struct spi_config cmd_config, data_config;
		/* We have to emulate 3 wire mode by packing the data/command
		 * bit into the upper bit of the SPI transfer.
		 * switch SPI to 9 bit mode, and write the transfer
		 */
		memcpy(&cmd_config, &dbi_config->config, sizeof(cmd_config));
		memcpy(&data_config, &dbi_config->config, sizeof(data_config));
		cmd_config.operation &= ~SPI_WORD_SIZE_MASK;
		cmd_config.operation |= SPI_WORD_SET(9);
		data_config.operation &= ~SPI_WORD_SIZE_MASK;
		data_config.operation |= SPI_WORD_SET(8);

		/* Send command */
		if (config->cd_invert) {
			data->spi_byte = BIT(9) | cmd;
		} else {
			data->spi_byte = cmd;
		}
		buffer.buf = &data->spi_byte;
		buffer.len = 1;
		ret = spi_write(config->spi_dev, &cmd_config, &buf_set);
		if (ret < 0) {
			goto out;
		}
		/* Now, we can switch to 8 bit mode, and read data */
		buffer.buf = (void *)response;
		buffer.len = len;
		ret = spi_read(config->spi_dev, &data_config, &buf_set);
	} else if (dbi_config->mode == MIPI_DBI_MODE_SPI_4WIRE) {
		/* 4 wire mode is much simpler. We just toggle the
		 * command/data GPIO to indicate if we are sending
		 * a command or data
		 */
		buffer.buf = &cmd;
		buffer.len = 1;

		gpio_pin_set_dt(&config->cmd_data, config->cd_invert);
		ret = spi_write(config->spi_dev, &dbi_config->config, &buf_set);
		if (ret < 0) {
			goto out;
		}

		buffer.buf = (void *)response;
		buffer.len = len;

		gpio_pin_set_dt(&config->cmd_data, !config->cd_invert);
		ret = spi_read(config->spi_dev, &dbi_config->config,
				&buf_set);
	} else {
		/* Otherwise, unsupported mode */
		ret = -ENOTSUP;
	}
out:
	k_spin_unlock(&data->lock, spinlock_key);
	return ret;
}


static int mipi_dbi_spi_reset(const struct device *dev, uint32_t delay)
{
	const struct mipi_dbi_spi_config *config = dev->config;
	int ret;

	if (config->reset.port == NULL) {
		return -ENOTSUP;
	}

	ret = gpio_pin_set_dt(&config->reset, config->rst_invert);
	if (ret < 0) {
		return ret;
	}
	k_msleep(delay);
	return gpio_pin_set_dt(&config->reset, !config->rst_invert);
}

static int mipi_dbi_spi_init(const struct device *dev)
{
	const struct mipi_dbi_spi_config *config = dev->config;
	int ret = 0;

	if (!device_is_ready(config->spi_dev)) {
		LOG_ERR("SPI device is not ready");
		return -ENODEV;
	}

	if (config->cmd_data.port) {
		ret = gpio_pin_configure_dt(&config->cmd_data, GPIO_OUTPUT);
		if (ret < 0) {
			LOG_ERR("Could not configure command/data GPIO (%d)", ret);
			return ret;
		}
	}

	if (config->reset.port) {
		ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Could not configure reset GPIO (%d)", ret);
			return ret;
		}
	}

	return ret;
}

static struct mipi_dbi_driver_api mipi_dbi_spi_driver_api = {
	.reset = mipi_dbi_spi_reset,
	.command_write = mipi_dbi_spi_command_write,
	.write_transfer = mipi_dbi_spi_write_transfer,
	.command_read = mipi_dbi_spi_command_read,
};

#define MIPI_DBI_SPI_INIT(n)							\
	static const struct mipi_dbi_spi_config					\
	    mipi_dbi_spi_config_##n = {						\
		    .spi_dev = DEVICE_DT_GET(					\
				    DT_INST_PHANDLE(n, spi_dev)),		\
		    .cmd_data = GPIO_DT_SPEC_INST_GET_OR(n, cd_gpios, {}),	\
		    .reset = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {}),	\
		    .cd_invert = DT_INST_PROP(n, cd_invert),			\
		    .rst_invert = DT_INST_PROP(n, rst_invert),			\
	};									\
	static struct mipi_dbi_spi_data mipi_dbi_spi_data_##n;			\
										\
	DEVICE_DT_INST_DEFINE(n, mipi_dbi_spi_init, NULL,			\
			&mipi_dbi_spi_data_##n,					\
			&mipi_dbi_spi_config_##n,				\
			POST_KERNEL,						\
			CONFIG_MIPI_DBI_INIT_PRIORITY,				\
			&mipi_dbi_spi_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MIPI_DBI_SPI_INIT)
