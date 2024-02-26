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
#include <soc.h>

#include "fwk_platform_ble.h"
#include "fwk_platform.h"

/* -------------------------------------------------------------------------- */
/*                                  Definitions                               */
/* -------------------------------------------------------------------------- */
#define DT_DRV_COMPAT nxp_hci_ble

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
LOG_MODULE_REGISTER(bt_driver);

#define HCI_IRQ_N DT_INST_IRQN(0)
#define HCI_IRQ_P DT_INST_IRQ(0, priority)

#define HCI_CMD         0x01
#define HCI_ACL	        0x02
#define HCI_EVT	        0x04

#if defined(HCI_NXP_SET_BDADDR)

#if !defined(BD_ADDR_OUI)
#define BD_ADDR_OUI                             0x37U, 0x60U, 0x00U
#endif

#define PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE      3U
#define	BLE_DEVICE_ADDR_SIZE                    (6U)
#define HCI_SET_MAC_ADDR_CMD_LENGTH             (8U)
#define HCI_CMD_PACKET_HEADER_LENGTH            (3U)
#define HCI_VENDOR_SPECIFIC_DBG_CMD             0x03FU
#define HCI_SET_MAC_ADDR_CMD                    0x0022U
#define BT_USER_BD 254
#define HCI_COMMAND(opCodeGroup, opCodeCommand)\
	(((uint16_t)(opCodeGroup) & (uint16_t)0x3FU)<<(uint16_t)SHIFT10) \
	|(uint16_t)((opCodeCommand) & 0x3FFU)

#endif /* HCI_NXP_SET_BDADDR */

/* IMU Interrupt priority */
#define RF_IMU0_IRQPriority                     2

extern int32_t ble_wakeup_handler(void);
/* -------------------------------------------------------------------------- */
/*                             Private functions                              */
/* -------------------------------------------------------------------------- */

static struct net_buf *bt_evt_recv(uint8_t *data, size_t len)
{
	struct net_buf *buf;
	uint8_t payload_len;
	uint8_t evt_hdr;
	bool discardable_evt;
	uint8_t subevt_type;
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

	/* Check if the event can be discarded */
	if (evt_hdr == BT_HCI_EVT_LE_META_EVENT) {
		subevt_type = data[BT_HCI_EVT_HDR_SIZE];
		if (subevt_type == BT_HCI_EVT_LE_ADVERTISING_REPORT) {
			discardable_evt = true;
		}
	}
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
		LOG_ERR("Event buffer is empty!");
		return NULL;
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

#if defined(HCI_NXP_SET_BDADDR)
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
	memcpy((void *)(bleDeviceAddress + PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE),
		(void *)addrOUI, PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE);
	memcpy((void *)&hciPacket[HCI_CMD_PACKET_HEADER_LENGTH + 2U],
		(const void *)bleDeviceAddress, BLE_DEVICE_ADDR_SIZE);
	hciBuffer[0] = HCI_CMD;
	memcpy(hciBuffer + 1U, (const void *)hciPacket, packetSize);

	PLATFORM_SendHciMessage(hciBuffer, packetSize + 1);
}
#endif /* HCI_NXP_SET_BDADDR */

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

	PLATFORM_SetHciRxCallback(hci_rx_cb);

	return 0;
}


static const struct bt_hci_driver drv = {
	.name		= "BT NXP",
	.open		= bt_nxp_open,
	.send		= bt_nxp_send,
	.bus		= BT_HCI_DRIVER_BUS_IPM,
};

static int bt_nxp_init(void)
{
	int status;
	int ret = 0;

	/*HCI Interrupt*/
	IRQ_CONNECT(HCI_IRQ_N, HCI_IRQ_P, ble_wakeup_handler, 0, 0);
	irq_enable(HCI_IRQ_N);

	do {
		status = PLATFORM_InitBle();
		if (status < 0) {
			LOG_ERR("BLE Controller initialization failed");
			ret = status;
			break;
		}
#if defined(HCI_NXP_SET_BDADDR)
		bt_nxp_set_address();
#endif /* HCI_NXP_SET_BDADDR */
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
