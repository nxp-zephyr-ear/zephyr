/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public APIs for MIPI-DBI drivers
 *
 * MIPI-DBI defines the following 3 interfaces:
 * Type A: Motorola 6800 type parallel bus
 * Type B: Intel 8080 type parallel bus
 * Type C: SPI Type (1 bit bus) with 3 options:
 *     1. 9 write clocks per byte, final bit is command/data selection bit
 *     2. Same as above, but 16 write clocks per byte
 *     3. 8 write clocks per byte. Command/data selected via GPIO pin
 * The current driver interface only supports type C modes 1 and 3
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MIPI_DBI_H_
#define ZEPHYR_INCLUDE_DRIVERS_MIPI_DBI_H_

/**
 * @brief MIPI-DBI driver APIs
 * @defgroup mipi_dbi_interface MIPI-DBI driver APIs
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/device.h>
#include <zephyr/display/mipi_display.h>
#include <zephyr/drivers/spi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SPI 3 wire (Mode C1). Uses 9 write clocks to send a byte of data.
 * The bit sent on the 9th clock indicates whether the byte is a
 * command or data byte
 */
#define MIPI_DBI_MODE_SPI_3WIRE 0x1
/**
 * SPI 4 wire (Mode C3). Uses 8 write clocks to send a byte of data.
 * an additional C/D pin will be use to indicate whether the byte is a
 * command or data byte
 */
#define MIPI_DBI_MODE_SPI_4WIRE 0x2

/**
 * @brief initialize a MIPI DBI SPI configuration struct from devicetree
 *
 * This helper allows drivers to initialize a MIPI DBI SPI configuration
 * structure using devicetree.
 * @param node_id Devicetree node identifier for the MIPI DBI device whose
 *                struct spi_config to create an initializer for
 * @param operation_ the desired operation field in the struct spi_config
 * @param delay_ the desired delay field in the struct spi_config's
 *               spi_cs_control, if there is one
 */
#define MIPI_DBI_SPI_CONFIG_DT(node_id, operation_, delay_)		\
	{								\
		.frequency = DT_PROP(node_id, spi_max_frequency),	\
		.operation = (operation_) |				\
			DT_PROP(node_id, duplex) |			\
			DT_PROP(node_id, frame_format) |			\
			COND_CODE_1(DT_PROP(node_id, spi_cpol), SPI_MODE_CPOL, (0)) |	\
			COND_CODE_1(DT_PROP(node_id, spi_cpha), SPI_MODE_CPHA, (0)) |	\
			COND_CODE_1(DT_PROP(node_id, spi_hold_cs), SPI_HOLD_ON_CS, (0)),	\
		.slave = DT_REG_ADDR(node_id),				\
		.cs = {							\
			.gpio = GPIO_DT_SPEC_GET_BY_IDX_OR(DT_PHANDLE(DT_PARENT(node_id), \
							   spi_dev), cs_gpios, \
							   DT_REG_ADDR(node_id), \
							   {}),		\
			.delay = (delay_),				\
		},							\
	}

/**
 * @brief MIPI DBI controller configuration
 *
 * Configuration for MIPI DBI controller write
 */
struct mipi_dbi_config {
	/** MIPI DBI mode (SPI 3 wire or 4 wire) */
	uint8_t mode;
	/** Pixel format */
	uint32_t pixfmt;
	/** SPI configuration */
	struct spi_config config;
};


/** MIPI-DBI host driver API */
__subsystem struct mipi_dbi_driver_api {
	int (*command_write)(const struct device *dev,
			     const struct mipi_dbi_config *config, uint8_t cmd,
			     const uint8_t *data, size_t len);
	int (*command_read)(const struct device *dev,
			    const struct mipi_dbi_config *config, uint8_t cmd,
			    uint8_t *response, size_t len);
	int (*write_transfer)(const struct device *dev,
			      const struct mipi_dbi_config *config,
			      const uint8_t *data, size_t len);
	int (*reset)(const struct device *dev, uint32_t delay);
};

/**
 * @brief Write a command to the display controller
 *
 * Writes a command, along with an optional data buffer to the display.
 * If data buffer and buffer length are NULL and 0 respectively, then
 * only a command will be sent.
 *
 * @param dev mipi dbi controller
 * @param config MIPI DBI configuration
 * @param cmd command to write to display controller
 * @param data optional data buffer to write after command
 * @param len size of data buffer in bytes. Set to 0 to skip sending data.
 * @retval 0 command write succeeded
 * @retval -EIO I/O error
 * @retval -ETIMEDOUT transfer timed out
 * @retval -EBUSY controller is busy
 * @retval -ENOSYS not implemented
 */
static inline int mipi_dbi_command_write(const struct device *dev,
					 const struct mipi_dbi_config *config,
					 uint8_t cmd, const uint8_t *data,
					 size_t len)
{
	const struct mipi_dbi_driver_api *api =
		(const struct mipi_dbi_driver_api *)dev->api;

	if (api->command_write == NULL) {
		return -ENOSYS;
	}
	return api->command_write(dev, config, cmd, data, len);
}

/**
 * @brief Read a command response from the display controller
 *
 * Reads a command response from the display controller.
 *
 * @param dev mipi dbi controller
 * @param config MIPI DBI configuration
 * @param cmd command to write to display controller
 * @param response response buffer, filled with display controller response
 * @param len size of response buffer in bytes.
 * @retval 0 command read succeeded
 * @retval -EIO I/O error
 * @retval -ETIMEDOUT transfer timed out
 * @retval -EBUSY controller is busy
 * @retval -ENOSYS not implemented
 */
static inline int mipi_dbi_command_read(const struct device *dev,
					const struct mipi_dbi_config *config,
					uint8_t cmd, uint8_t *response,
					size_t len)
{
	const struct mipi_dbi_driver_api *api =
		(const struct mipi_dbi_driver_api *)dev->api;

	if (api->command_read == NULL) {
		return -ENOSYS;
	}
	return api->command_read(dev, config, cmd, response, len);
}

/**
 * @brief Write a data buffer to the display controller
 *
 * Writes a data buffer to the display controller, without any associated
 * command. This can be used to chunk a large data transfer for one DCS
 * command into multiple discrete transfers.
 *
 * @param dev mipi dbi controller
 * @param config MIPI DBI configuration
 * @param data data buffer to write
 * @param len size of data buffer in bytes
 * @retval 0 buffer write succeeded.
 * @retval -EIO I/O error
 * @retval -ETIMEDOUT transfer timed out
 * @retval -EBUSY controller is busy
 * @retval -ENOSYS not implemented
 */
static inline int mipi_dbi_write_transfer(const struct device *dev,
					  const struct mipi_dbi_config *config,
					  const uint8_t *data, size_t len)
{
	const struct mipi_dbi_driver_api *api =
		(const struct mipi_dbi_driver_api *)dev->api;

	if (api->write_transfer == NULL) {
		return -ENOSYS;
	}
	return api->write_transfer(dev, config, data, len);
}

/**
 * @brief Resets attached display controller
 *
 * Resets the attached display controller.
 * @param dev mipi dbi controller
 * @param delay duration to set reset signal for, in milliseconds
 * @retval 0 reset succeeded
 * @retval -EIO I/O error
 * @retval -ENOSYS not implemented
 * @retval -ENOTSUP not supported
 */
static inline int mipi_dbi_reset(const struct device *dev, uint32_t delay)
{
	const struct mipi_dbi_driver_api *api =
		(const struct mipi_dbi_driver_api *)dev->api;

	if (api->reset == NULL) {
		return -ENOSYS;
	}
	return api->reset(dev, delay);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_MIPI_DBI_H_ */
