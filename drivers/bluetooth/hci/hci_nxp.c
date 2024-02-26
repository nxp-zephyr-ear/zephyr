/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <zephyr/init.h>
#include <zephyr/drivers/bluetooth/hci_driver.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/bluetooth/hci_types.h>
#include <soc.h>

#include "fwk_platform_ble.h"
#include "fwk_platform.h"

/* -------------------------------------------------------------------------- */
/*                                  Definitions                               */
/* -------------------------------------------------------------------------- */

#define DT_DRV_COMPAT nxp_hci_ble

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
LOG_MODULE_REGISTER(bt_driver);

#define HCI_IRQ_N        DT_INST_IRQ_BY_IDX(0, 0, irq)
#define HCI_IRQ_P        DT_INST_IRQ_BY_IDX(0, 0, priority)
#define HCI_WAKEUP_IRQ_N DT_INST_IRQ_BY_IDX(0, 1, irq)
#define HCI_WAKEUP_IRQ_P DT_INST_IRQ_BY_IDX(0, 1, priority)

#define HCI_CMD 0x01
#define HCI_ACL 0x02
#define HCI_EVT 0x04

/* Vendor specific commands */
#define HCI_CMD_PACKET_HEADER_LENGTH                    3U
#define HCI_CMD_VENDOR_OCG                              0x3FU
#define HCI_CMD_STORE_BT_CAL_DATA_OCF                   0x61U
#define HCI_CMD_STORE_BT_CAL_DATA_PARAM_LENGTH          32U
#define HCI_CMD_STORE_BT_CAL_DATA_ANNEX100_OCF          0xFFU
#define HCI_CMD_STORE_BT_CAL_DATA_PARAM_ANNEX100_LENGTH 16U
#define HCI_CMD_SET_BT_SLEEP_MODE_OCF                   0x23U
#define HCI_CMD_SET_BT_SLEEP_MODE_PARAM_LENGTH          3U
#define HCI_CMD_BT_HIU_HS_ENABLE_OCF                    0x5A
#define HCI_CMD_BT_HOST_SLEEP_CONFIG_OCF                0x59U
#define HCI_CMD_BT_HOST_SLEEP_CONFIG_PARAM_LENGTH       2U

#if defined(HCI_NXP_SET_BDADDR)

#if !defined(BD_ADDR_OUI)
#define BD_ADDR_OUI 0x37U, 0x60U, 0x00U
#endif

#define PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE 3U
#define BLE_DEVICE_ADDR_SIZE               (6U)
#define HCI_SET_MAC_ADDR_CMD_LENGTH        (8U)
#define HCI_CMD_PACKET_HEADER_LENGTH       (3U)
#define HCI_VENDOR_SPECIFIC_DBG_CMD        0x03FU
#define HCI_SET_MAC_ADDR_CMD               0x0022U
#define BT_USER_BD                         254
#define HCI_COMMAND(opCodeGroup, opCodeCommand)                                                    \
	(((uint16_t)(opCodeGroup) & (uint16_t)0x3FU) << (uint16_t)SHIFT10) |                       \
		(uint16_t)((opCodeCommand)&0x3FFU)

#endif /* HCI_NXP_SET_BDADDR */

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

extern int32_t ble_hci_handler(void);
extern int32_t ble_wakeup_done_handler(void);

/* -------------------------------------------------------------------------- */
/*                             Private functions                              */
/* -------------------------------------------------------------------------- */

#if CONFIG_HCI_NXP_ENABLE_AUTO_SLEEP || CONFIG_HCI_NXP_SET_CAL_DATA
static int nxp_bt_send_vs_command(uint16_t opcode, const uint8_t *params, uint8_t params_len)
{
	struct net_buf *buf;

	/* Allocate buffer for the hci command */
	buf = bt_hci_cmd_create(opcode, params_len);
	if (buf == NULL) {
		LOG_ERR("Unable to allocate command buffer");
		return -ENOMEM;
	}

	/* Add data part of packet */
	net_buf_add_mem(buf, params, params_len);

	/* Send the command */
	return bt_hci_cmd_send_sync(opcode, buf, NULL);
}
#endif /* CONFIG_HCI_NXP_ENABLE_AUTO_SLEEP || CONFIG_HCI_NXP_SET_CAL_DATA */

#if CONFIG_HCI_NXP_ENABLE_AUTO_SLEEP
static int nxp_bt_enable_controller_autosleep(void)
{
	uint16_t opcode = BT_OP(BT_OGF_VS, HCI_CMD_SET_BT_SLEEP_MODE_OCF);
	const uint8_t params[HCI_CMD_SET_BT_SLEEP_MODE_PARAM_LENGTH] = {
		0x02U, /* Auto sleep enable */
		0x00U, /* Idle timeout LSB */
		0x00U  /* Idle timeout MSB */
	};

	/* Send the command */
	return nxp_bt_send_vs_command(opcode, params, HCI_CMD_SET_BT_SLEEP_MODE_PARAM_LENGTH);
}

static int nxp_bt_set_host_sleep_config(void)
{
	uint16_t opcode = BT_OP(BT_OGF_VS, HCI_CMD_BT_HOST_SLEEP_CONFIG_OCF);
	const uint8_t params[HCI_CMD_BT_HOST_SLEEP_CONFIG_PARAM_LENGTH] = {
		0xFFU, /* BT_HIU_WAKEUP_INBAND */
		0xFFU, /* BT_HIU_WAKE_GAP_WAIT_FOR_IRQ */
	};

	/* Send the command */
	return nxp_bt_send_vs_command(opcode, params, HCI_CMD_BT_HOST_SLEEP_CONFIG_PARAM_LENGTH);
}
#endif /* CONFIG_HCI_NXP_ENABLE_AUTO_SLEEP */

#if CONFIG_HCI_NXP_SET_CAL_DATA
static int bt_nxp_set_calibration_data(void)
{
	uint16_t opcode = BT_OP(BT_OGF_VS, HCI_CMD_STORE_BT_CAL_DATA_OCF);
	extern const uint8_t hci_cal_data_params[HCI_CMD_STORE_BT_CAL_DATA_PARAM_LENGTH];

	/* Send the command */
	return nxp_bt_send_vs_command(opcode, hci_cal_data_params,
				      HCI_CMD_STORE_BT_CAL_DATA_PARAM_LENGTH);
}

#if CONFIG_HCI_NXP_SET_CAL_DATA_ANNEX100
static int bt_nxp_set_calibration_data_annex100(void)
{
	uint16_t opcode = BT_OP(BT_OGF_VS, HCI_CMD_STORE_BT_CAL_DATA_ANNEX100_OCF);

	extern const uint8_t
		hci_cal_data_annex100_params[HCI_CMD_STORE_BT_CAL_DATA_PARAM_ANNEX100_LENGTH];

	/* Send the command */
	return nxp_bt_send_vs_command(opcode, hci_cal_data_annex100_params,
				      HCI_CMD_STORE_BT_CAL_DATA_PARAM_ANNEX100_LENGTH);
}
#endif /* CONFIG_HCI_NXP_SET_CAL_DATA_ANNEX100 */

#endif /* CONFIG_HCI_NXP_SET_CAL_DATA */

#ifdef CONFIG_HCI_NXP_SET_BDADDR
static void bt_nxp_set_address(void)
{
	uint8_t bleDeviceAddress[BLE_DEVICE_ADDR_SIZE] = {0};
	uint8_t uid[16] = {0};
	uint8_t uuidLen;
	uint8_t addrOUI[PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE] = {BD_ADDR_OUI};
	/* Set BD address by HCI message */
	uint8_t hciPacket[HCI_SET_MAC_ADDR_CMD_LENGTH + HCI_CMD_PACKET_HEADER_LENGTH];
	uint16_t opcode = HCI_COMMAND(HCI_VENDOR_SPECIFIC_DBG_CMD, HCI_SET_MAC_ADDR_CMD);
	uint8_t hciBuffer[HCI_SET_MAC_ADDR_CMD_LENGTH + HCI_CMD_PACKET_HEADER_LENGTH + 1U];
	uint32_t packetSize = HCI_CMD_PACKET_HEADER_LENGTH + HCI_SET_MAC_ADDR_CMD_LENGTH;

	memcpy((void *)hciPacket, (const void *)&opcode, 2U);
	/* Set HCI parameter length */
	hciPacket[2] = (uint8_t)HCI_SET_MAC_ADDR_CMD_LENGTH;
	/* Set command parameter ID */
	hciPacket[3] = (uint8_t)BT_USER_BD;
	/* Set command parameter length */
	hciPacket[4] = (uint8_t)6U;
	/* Use UID to create a unique MAC address */
	PLATFORM_GetMCUUid(uid, &uuidLen);
	/* Set 3 LSB of MAC address from UUID */
	if (uuidLen >= PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE) {
		memcpy((void *)bleDeviceAddress,
		       (void *)(uid + uuidLen - PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE),
		       PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE);
	}
	/* Set 3 MSB of MAC address from OUI */
	memcpy((void *)(bleDeviceAddress + PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE), (void *)addrOUI,
	       PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE);
	memcpy((void *)&hciPacket[HCI_CMD_PACKET_HEADER_LENGTH + 2U],
	       (const void *)bleDeviceAddress, BLE_DEVICE_ADDR_SIZE);
	hciBuffer[0] = HCI_CMD;
	memcpy(hciBuffer + 1U, (const void *)hciPacket, packetSize);

	PLATFORM_SendHciMessage(hciBuffer, packetSize + 1);
}
#endif /* HCI_NXP_SET_BDADDR */

static bool is_hci_event_discardable(const uint8_t *evt_data)
{
	bool ret = false;
	uint8_t evt_type = evt_data[0];

	switch (evt_type) {
	case BT_HCI_EVT_LE_META_EVENT: {
		uint8_t subevt_type = evt_data[sizeof(struct bt_hci_evt_hdr)];

		switch (subevt_type) {
		case BT_HCI_EVT_LE_ADVERTISING_REPORT:
			ret = true;
			break;
		default:
			break;
		}
	} break;

	default:
		break;
	}

	return ret;
}

static struct net_buf *bt_evt_recv(uint8_t *data, size_t len)
{
	struct net_buf *buf;
	uint8_t payload_len;
	uint8_t evt_hdr;
	bool discardable_evt;
	size_t space_in_buffer;

	payload_len = data[1];
	evt_hdr = data[0];
	discardable_evt = false;

	/* Data Check */
	if (len < BT_HCI_EVT_HDR_SIZE) {
		LOG_ERR("Event header is missing");
		return NULL;
	}
	if ((len - BT_HCI_EVT_HDR_SIZE) != payload_len) {
		LOG_ERR("Event payload length is incorrect");
		return NULL;
	}

	discardable_evt = is_hci_event_discardable(data);

	/* Allocate a buffer for the HCI Event */
	buf = bt_buf_get_evt(evt_hdr, discardable_evt, K_NO_WAIT);

	if (buf) {
		space_in_buffer = net_buf_tailroom(buf);
		if (len > space_in_buffer) {
			LOG_ERR("Buffer size error,INFO : evt_hdr=%d, data_len=%zu, buf_size=%zu",
				evt_hdr, len, space_in_buffer);
			net_buf_unref(buf);
			return NULL;
		}
		/* Copy the data to the buffer */
		net_buf_add_mem(buf, data, len);
	} else {
		if (discardable_evt) {
			LOG_DBG("Discardable buffer pool full, ignoring event");
		} else {
			LOG_ERR("No available event buffers!");
		}
		return buf;
	}

	return buf;
}

static struct net_buf *bt_acl_recv(uint8_t *data, size_t len)
{
	struct net_buf *buf;
	uint16_t payload_len;

	/* Data Check */
	if (len < BT_HCI_ACL_HDR_SIZE) {
		LOG_ERR("ACL header is missing");
		return NULL;
	}
	memcpy((void *)&payload_len, (void *)&data[2], 2);
	if ((len - BT_HCI_ACL_HDR_SIZE) != payload_len) {
		LOG_ERR("ACL payload length is incorrect");
		return NULL;
	}
	/* Allocate a buffer for the received data */
	buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_NO_WAIT);

	if (buf) {
		if (len > net_buf_tailroom(buf)) {
			LOG_ERR("Buffer doesn't have enough space to store the data");
			net_buf_unref(buf);
			return NULL;
		}
		/* Copy the data to the buffer */
		net_buf_add_mem(buf, data, len);
	} else {
		LOG_ERR("ACL buffer is empty");
		return NULL;
	}

	return buf;
}

static void hci_rx_cb(uint8_t packetType, uint8_t *data, uint16_t len)
{
	struct net_buf *buf;

	switch (packetType) {
	case HCI_EVT:
		/* create a buffer and fill it out with event data  */
		buf = bt_evt_recv(data, len);
		break;

	case HCI_ACL:
		/* create a buffer and fill it out with ACL data  */
		buf = bt_acl_recv(data, len);
		break;

	default:
		buf = NULL;
		LOG_ERR("Unknown HCI type");
	}

	if (buf) {
		/* Provide the buffer to the host */
		bt_recv(buf);
	}
}

static int bt_nxp_send(struct net_buf *buf)
{
	uint8_t packetType;

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_CMD:
		packetType = HCI_CMD;
		break;

	case BT_BUF_ACL_OUT:
		packetType = HCI_ACL;
		break;

	default:
		LOG_ERR("Not supported type");
		return -1;
	}

	net_buf_push_u8(buf, packetType);
	PLATFORM_SendHciMessage(buf->data, buf->len);

	net_buf_unref(buf);

	return 0;
}

static int bt_nxp_open(void)
{
	int ret = 0;

	do {
		ret = PLATFORM_SetHciRxCallback(hci_rx_cb);
		if (ret < 0) {
			LOG_ERR("BLE HCI RX callback registration failed");
			break;
		}

		ret = PLATFORM_StartHci();
		if (ret < 0) {
			LOG_ERR("HCI open failed");
			break;
		}

#if CONFIG_HCI_NXP_SET_BDADDR
		bt_nxp_set_address();
#endif /* HCI_NXP_SET_BDADDR */

#if CONFIG_HCI_NXP_SET_CAL_DATA
		ret = bt_nxp_set_calibration_data();
		if (ret < 0) {
			LOG_ERR("Failed to set calibration data");
			break;
		}
#if CONFIG_HCI_NXP_SET_CAL_DATA_ANNEX100
		/* After send annex55 to CPU2, CPU2 need reset,
		 * a delay of at least 20ms is required to continue sending annex100
		 */
		k_sleep(Z_TIMEOUT_MS(20));

		ret = bt_nxp_set_calibration_data_annex100();
		if (ret < 0) {
			LOG_ERR("Failed to set calibration data");
			break;
		}
#endif /* CONFIG_HCI_NXP_SET_CAL_DATA_ANNEX100 */
#endif /* CONFIG_HCI_NXP_SET_CAL_DATA */

#if CONFIG_HCI_NXP_ENABLE_AUTO_SLEEP
		ret = nxp_bt_set_host_sleep_config();
		if (ret < 0) {
			LOG_ERR("Failed to set host sleep config");
			break;
		}

		ret = nxp_bt_enable_controller_autosleep();
		if (ret < 0) {
			LOG_ERR("Failed to configure controller autosleep");
			break;
		}
#endif
	} while (false);

	return ret;
}

static int bt_nxp_close(void)
{
	/* Do nothing */

	return 0;
}

static const struct bt_hci_driver drv = {
	.name = "BT NXP",
	.open = bt_nxp_open,
	.close = bt_nxp_close,
	.send = bt_nxp_send,
	.bus = BT_HCI_DRIVER_BUS_IPM,
};

static int bt_nxp_init(void)
{
	int status;
	int ret = 0;

	/* HCI Interrupt */
	IRQ_CONNECT(HCI_IRQ_N, HCI_IRQ_P, ble_hci_handler, 0, 0);
	irq_enable(HCI_IRQ_N);

	/* Wake up done interrupt */
	IRQ_CONNECT(HCI_WAKEUP_IRQ_N, HCI_WAKEUP_IRQ_P, ble_wakeup_done_handler, 0, 0);
	irq_enable(HCI_WAKEUP_IRQ_N);

	do {
		status = PLATFORM_InitBle();
		if (status < 0) {
			LOG_ERR("BLE Controller initialization failed");
			ret = status;
			break;
		}

		status = bt_hci_driver_register(&drv);
		if (status < 0) {
			LOG_ERR("HCI driver registration failed");
			ret = status;
			break;
		}
	} while (0);

	return ret;
}

SYS_INIT(bt_nxp_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
