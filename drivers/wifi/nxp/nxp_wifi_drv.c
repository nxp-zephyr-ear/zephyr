/**
 * Copyright 2023-2024 NXP
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file nxp_wifi_drv.c
 * Shim layer between wifi driver connection manager and zephyr
 * ethernet L2 layer
 */

#include <zephyr/net/ethernet.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/device.h>
#include <soc.h>
#include <ethernet/eth_stats.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_nxp, CONFIG_WIFI_LOG_LEVEL);

#include "wlan_bt_fw.h"
#include "wlan.h"
#include "wpl.h"
#include "wm_net.h"
#include "dhcp-server.h"
#include "wifi_shell.h"
#ifdef CONFIG_WPA_SUPP
#include "wifi_nxp.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MAX_JSON_NETWORK_RECORD_LENGTH 185

#define WPL_SYNC_TIMEOUT_MS K_FOREVER

#define UAP_NETWORK_NAME "uap-network"

#define EVENT_BIT(event) (1 << event)

#define WPL_SYNC_INIT_GROUP                                                                        \
	EVENT_BIT(WLAN_REASON_INITIALIZED) | EVENT_BIT(WLAN_REASON_INITIALIZATION_FAILED)

#define WPL_SYNC_CONNECT_GROUP                                                                     \
	EVENT_BIT(WLAN_REASON_SUCCESS) | EVENT_BIT(WLAN_REASON_CONNECT_FAILED) |                   \
		EVENT_BIT(WLAN_REASON_NETWORK_NOT_FOUND) |                                         \
		EVENT_BIT(WLAN_REASON_NETWORK_AUTH_FAILED) | EVENT_BIT(WLAN_REASON_ADDRESS_FAILED)

#define WPL_SYNC_DISCONNECT_GROUP EVENT_BIT(WLAN_REASON_USER_DISCONNECT)

#define WPL_SYNC_UAP_START_GROUP                                                                   \
	EVENT_BIT(WLAN_REASON_UAP_SUCCESS) | EVENT_BIT(WLAN_REASON_UAP_START_FAILED)

#define WPL_SYNC_UAP_STOP_GROUP                                                                    \
	EVENT_BIT(WLAN_REASON_UAP_STOPPED) | EVENT_BIT(WLAN_REASON_UAP_STOP_FAILED)

#define EVENT_SCAN_DONE     23
#define WPL_SYNC_SCAN_GROUP EVENT_BIT(EVENT_SCAN_DONE)

typedef enum _wpl_state {
	WPL_NOT_INITIALIZED,
	WPL_INITIALIZED,
	WPL_STARTED,
} wpl_state_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
static wpl_state_t s_wplState = WPL_NOT_INITIALIZED;
static bool s_wplStaConnected = false;
static bool s_wplUapActivated = false;
static struct k_event s_wplSyncEvent;
static linkLostCb_t s_linkLostCb = NULL;

static struct wlan_network sta_network;
static struct wlan_network uap_network;

extern interface_t g_mlan;
extern interface_t g_uap;

#ifdef CONFIG_WPA_SUPP
extern const rtos_wpa_supp_dev_ops wpa_supp_ops;
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
int wlan_event_callback(enum wlan_event_reason reason, void *data);
static int WLP_process_results(unsigned int count);
static void LinkStatusChangeCallback(bool linkState);
extern int low_level_output(const struct device *dev, struct net_pkt *pkt);

static void printSeparator(void)
{
	LOG_INF("========================================");
}

/* Callback Function passed to WLAN Connection Manager. The callback function
 * gets called when there are WLAN Events that need to be handled by the
 * application.
 */
int wlan_event_callback(enum wlan_event_reason reason, void *data)
{
	int ret;
	struct wlan_ip_config addr;
	char ip[16];
	static int auth_fail = 0;
	wlan_uap_client_disassoc_t *disassoc_resp = data;

	printSeparator();
	LOG_INF("app_cb: WLAN: received event %d", reason);
	printSeparator();

	if (s_wplState >= WPL_INITIALIZED) {
		k_event_set(&s_wplSyncEvent, EVENT_BIT(reason));
	}

	switch (reason) {
	case WLAN_REASON_INITIALIZED:
		LOG_INF("app_cb: WLAN initialized");
		printSeparator();

		ret = wlan_basic_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize BASIC WLAN CLIs");
			return 0;
		}

		ret = wlan_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize WLAN CLIs");
			return 0;
		}
		LOG_INF("WLAN CLIs are initialized");
		printSeparator();
#ifdef RW610
		ret = wlan_enhanced_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize WLAN CLIs");
			return 0;
		}
		LOG_INF("ENHANCED WLAN CLIs are initialized");
		printSeparator();

#ifdef CONFIG_HOST_SLEEP
		ret = host_sleep_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize WLAN CLIs");
			return 0;
		}
		LOG_INF("HOST SLEEP CLIs are initialized");
		printSeparator();
#endif
#endif
		help_command(0, NULL);
		printSeparator();
		break;
	case WLAN_REASON_INITIALIZATION_FAILED:
		LOG_ERR("app_cb: WLAN: initialization failed");
		break;
	case WLAN_REASON_AUTH_SUCCESS:
		net_eth_carrier_on(g_mlan.netif);
		LOG_INF("app_cb: WLAN: authenticated to network");
		break;
	case WLAN_REASON_SUCCESS:
		LOG_INF("app_cb: WLAN: connected to network");
		ret = wlan_get_address(&addr);
		if (ret != WM_SUCCESS) {
			LOG_ERR("failed to get IP address");
			return 0;
		}

		net_inet_ntoa(addr.ipv4.address, ip);

		ret = wlan_get_current_network(&sta_network);
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to get External AP network");
			return 0;
		}

		LOG_INF("Connected to following BSS:");
		LOG_INF("SSID = [%s]", sta_network.ssid);
		if (addr.ipv4.address != 0U) {
			LOG_INF("IPv4 Address: [%s]", ip);
		}
#ifdef CONFIG_IPV6
		int i;
#ifndef CONFIG_ZEPHYR
		for (i = 0; i < CONFIG_MAX_IPV6_ADDRESSES; i++) {
			if (ip6_addr_isvalid(addr.ipv6[i].addr_state))
#else
		for (i = 0; i < CONFIG_MAX_IPV6_ADDRESSES && i < sta_network.ip.ipv6_count; i++) {
			if ((addr.ipv6[i].addr_state == NET_ADDR_TENTATIVE) ||
			    (addr.ipv6[i].addr_state == NET_ADDR_PREFERRED))
#endif
			{
				LOG_INF("IPv6 Address: %-13s:\t%s (%s)",
					ipv6_addr_type_to_desc(&addr.ipv6[i]),
					ipv6_addr_addr_to_desc(&addr.ipv6[i]),
					ipv6_addr_state_to_desc(addr.ipv6[i].addr_state));
			}
		}
		LOG_INF("");
#endif
		auth_fail = 0;
		break;
	case WLAN_REASON_CONNECT_FAILED:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("app_cb: WLAN: connect failed");
		break;
	case WLAN_REASON_NETWORK_NOT_FOUND:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("app_cb: WLAN: network not found");
		break;
	case WLAN_REASON_NETWORK_AUTH_FAILED:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("app_cb: WLAN: network authentication failed");
		auth_fail++;
		if (auth_fail >= 3) {
			LOG_WRN("Authentication Failed. Disconnecting ... ");
			wlan_disconnect();
			auth_fail = 0;
		}
		break;
	case WLAN_REASON_ADDRESS_SUCCESS:
		LOG_INF("network mgr: DHCP new lease");
		break;
	case WLAN_REASON_ADDRESS_FAILED:
		LOG_WRN("app_cb: failed to obtain an IP address");
		break;
	case WLAN_REASON_USER_DISCONNECT:
		net_eth_carrier_off(g_mlan.netif);
		LOG_INF("app_cb: disconnected");
		auth_fail = 0;
		break;
	case WLAN_REASON_LINK_LOST:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("app_cb: WLAN: link lost");
		break;
	case WLAN_REASON_CHAN_SWITCH:
		LOG_INF("app_cb: WLAN: channel switch");
		break;
	case WLAN_REASON_UAP_SUCCESS:
		net_eth_carrier_on(g_uap.netif);
		LOG_INF("app_cb: WLAN: UAP Started");
		ret = wlan_get_current_uap_network(&uap_network);

		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to get Soft AP network");
			return 0;
		}

		printSeparator();
		LOG_INF("Soft AP \"%s\" started successfully", uap_network.ssid);
		printSeparator();
		if (dhcp_server_start(net_get_uap_handle())) {
			LOG_ERR("Error in starting dhcp server");
			break;
		}

		LOG_INF("DHCP Server started successfully");
		printSeparator();
		break;
	case WLAN_REASON_UAP_CLIENT_ASSOC:
		LOG_INF("app_cb: WLAN: UAP a Client Associated");
		printSeparator();
		LOG_INF("Client => ");
		print_mac((const char *)data);
		LOG_INF("Associated with Soft AP");
		printSeparator();
		break;
	case WLAN_REASON_UAP_CLIENT_CONN:
		LOG_INF("app_cb: WLAN: UAP a Client Connected");
		printSeparator();
		LOG_INF("Client => ");
		print_mac((const char *)data);
		LOG_INF("Connected with Soft AP");
		printSeparator();
		break;
	case WLAN_REASON_UAP_CLIENT_DISSOC:
		printSeparator();
		LOG_INF("app_cb: WLAN: UAP a Client Dissociated:");
		LOG_INF(" Client MAC => ");
		print_mac((const char *)(disassoc_resp->sta_addr));
		LOG_INF(" Reason code => ");
		LOG_INF("%d", disassoc_resp->reason_code);
		printSeparator();
		break;
	case WLAN_REASON_UAP_STOPPED:
		net_eth_carrier_off(g_uap.netif);
		LOG_INF("app_cb: WLAN: UAP Stopped");
		printSeparator();
		LOG_INF("Soft AP \"%s\" stopped successfully", uap_network.ssid);
		printSeparator();

		dhcp_server_stop();

		LOG_INF("DHCP Server stopped successfully");
		printSeparator();
		break;
	case WLAN_REASON_PS_ENTER:
		LOG_INF("app_cb: WLAN: PS_ENTER");
		break;
	case WLAN_REASON_PS_EXIT:
		LOG_INF("app_cb: WLAN: PS EXIT");
		break;
#ifdef CONFIG_SUBSCRIBE_EVENT_SUPPORT
	case WLAN_REASON_RSSI_HIGH:
	case WLAN_REASON_SNR_LOW:
	case WLAN_REASON_SNR_HIGH:
	case WLAN_REASON_MAX_FAIL:
	case WLAN_REASON_BEACON_MISSED:
	case WLAN_REASON_DATA_RSSI_LOW:
	case WLAN_REASON_DATA_RSSI_HIGH:
	case WLAN_REASON_DATA_SNR_LOW:
	case WLAN_REASON_DATA_SNR_HIGH:
	case WLAN_REASON_LINK_QUALITY:
	case WLAN_REASON_PRE_BEACON_LOST:
		break;
#endif
	default:
		LOG_WRN("app_cb: WLAN: Unknown Event: %d", reason);
	}
	return 0;
}

/* Link lost callback */
static void LinkStatusChangeCallback(bool linkState)
{
	if (linkState == false) {
		LOG_WRN("-------- LINK LOST --------");
	} else {
		LOG_WRN("-------- LINK REESTABLISHED --------");
	}
}

int WPL_Init(void)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;

	if (s_wplState != WPL_NOT_INITIALIZED) {
		status = WPLRET_FAIL;
	}

	if (status == WPLRET_SUCCESS) {
		k_event_init(&s_wplSyncEvent);
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_init(wlan_fw_bin, wlan_fw_bin_len);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplState = WPL_INITIALIZED;
	}

	if (status != WPLRET_SUCCESS) {
		return -1;
	}

	return 0;
}

int WPL_Start(linkLostCb_t callbackFunction)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;

	if (s_wplState != WPL_INITIALIZED) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		k_event_clear(&s_wplSyncEvent, WPL_SYNC_INIT_GROUP);

		ret = wlan_start(wlan_event_callback);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_INIT_GROUP, true,
				       WPL_SYNC_TIMEOUT_MS);
		if (syncBit & EVENT_BIT(WLAN_REASON_INITIALIZED)) {
			s_linkLostCb = callbackFunction;
			status = WPLRET_SUCCESS;
		} else if (syncBit & EVENT_BIT(WLAN_REASON_INITIALIZATION_FAILED)) {
			status = WPLRET_FAIL;
		} else {
			status = WPLRET_TIMEOUT;
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplState = WPL_STARTED;
	}

	if (status != WPLRET_SUCCESS) {
		return -1;
	}

	return 0;
}

int WPL_Stop(void)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_stop();
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplState = WPL_INITIALIZED;
	}

	if (status != WPLRET_SUCCESS) {
		return -1;
	}

	return 0;
}

static int WPL_Start_AP(const struct device *dev, struct wifi_connect_req_params *params)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;
	interface_t *if_handle = (interface_t *)dev->data;

	if (if_handle->state.interface != WLAN_BSS_TYPE_UAP) {
		LOG_ERR("Wi-Fi not in uAP mode");
		return -EIO;
	}

	if ((s_wplState != WPL_STARTED) || (s_wplUapActivated != false)) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		if ((params->ssid_length == 0) || (params->ssid_length > IEEEtypes_SSID_SIZE)) {
			status = WPLRET_BAD_PARAM;
		}
	}

	if (status == WPLRET_SUCCESS) {
		wlan_remove_network(uap_network.name);

		wlan_initialize_uap_network(&uap_network);

		memcpy(uap_network.ssid, params->ssid, params->ssid_length);
		uap_network.channel = params->channel;

		if (params->mfp == WIFI_MFP_REQUIRED) {
			uap_network.security.mfpc = true;
			uap_network.security.mfpr = true;
		} else if (params->mfp == WIFI_MFP_OPTIONAL) {
			uap_network.security.mfpc = true;
			uap_network.security.mfpr = false;
		}

		if (params->security == WIFI_SECURITY_TYPE_NONE) {
			uap_network.security.type = WLAN_SECURITY_NONE;
		} else if (params->security == WIFI_SECURITY_TYPE_PSK) {
			uap_network.security.type = WLAN_SECURITY_WPA2;
			uap_network.security.psk_len = params->psk_length;
			strncpy(uap_network.security.psk, params->psk, params->psk_length);
		}
#ifdef CONFIG_WPA_SUPP
		else if (params->security == WIFI_SECURITY_TYPE_PSK_SHA256) {
			uap_network.security.type = WLAN_SECURITY_WPA2_SHA256;
			uap_network.security.psk_len = params->psk_length;
			strncpy(uap_network.security.psk, params->psk, params->psk_length);
		}
#endif
		else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			uap_network.security.type = WLAN_SECURITY_WPA3_SAE;
			uap_network.security.password_len = params->sae_password_length;
			strncpy(uap_network.security.password, params->sae_password,
				params->sae_password_length);
		} else {
			status = WPLRET_BAD_PARAM;
		}
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_add_network(&uap_network);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		k_event_clear(&s_wplSyncEvent, WPL_SYNC_UAP_START_GROUP);

		ret = wlan_start_network(uap_network.name);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		} else {
			syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_UAP_START_GROUP, true,
					       WPL_SYNC_TIMEOUT_MS);
			if (syncBit & EVENT_BIT(WLAN_REASON_UAP_SUCCESS)) {
				status = WPLRET_SUCCESS;
			} else if (syncBit & EVENT_BIT(WLAN_REASON_UAP_START_FAILED)) {
				status = WPLRET_FAIL;
			} else {
				status = WPLRET_TIMEOUT;
			}
		}

		if (status != WPLRET_SUCCESS) {
			wlan_remove_network(uap_network.name);
		}
	}

	if (status == WPLRET_SUCCESS) {
		ret = dhcp_server_start(net_get_uap_handle());
		if (ret != WM_SUCCESS) {
			wlan_stop_network(uap_network.name);
			wlan_remove_network(uap_network.name);
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplUapActivated = true;
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to enable Wi-Fi AP mode");
		return -EAGAIN;
	}

	return 0;
}

static int WPL_Stop_AP(const struct device *dev)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;

	if ((s_wplState != WPL_STARTED) || (s_wplUapActivated != true)) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		dhcp_server_stop();

		k_event_clear(&s_wplSyncEvent, WPL_SYNC_UAP_START_GROUP);

		ret = wlan_stop_network(UAP_NETWORK_NAME);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		} else {
			syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_UAP_STOP_GROUP, true,
					       WPL_SYNC_TIMEOUT_MS);
			if (syncBit & EVENT_BIT(WLAN_REASON_UAP_STOPPED)) {
				status = WPLRET_SUCCESS;
			} else if (syncBit & EVENT_BIT(WLAN_REASON_UAP_STOP_FAILED)) {
				status = WPLRET_FAIL;
			} else {
				status = WPLRET_TIMEOUT;
			}
		}
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_remove_network(UAP_NETWORK_NAME);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplUapActivated = false;
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to disable Wi-Fi AP mode");
		return -EAGAIN;
	}

	return 0;
}

static int WLP_process_results(unsigned int count)
{
	struct wlan_scan_result scan_result = {0};
	struct wifi_scan_result res = {0};

	if (!count) {
		LOG_INF("No Wi-Fi AP found");
		goto out;
	}

	for (int i = 0; i < count; i++) {
		wlan_get_scan_result(i, &scan_result);

		memset(&res, 0, sizeof(struct wifi_scan_result));

		memcpy(res.mac, scan_result.bssid, WIFI_MAC_ADDR_LEN);
		res.mac_length = WIFI_MAC_ADDR_LEN;
		res.ssid_length = scan_result.ssid_len;
		strncpy(res.ssid, scan_result.ssid, scan_result.ssid_len);

		res.rssi = -scan_result.rssi;
		res.channel = scan_result.channel;
		res.band = scan_result.channel > 14 ? WIFI_FREQ_BAND_5_GHZ : WIFI_FREQ_BAND_2_4_GHZ;

		res.security = WIFI_SECURITY_TYPE_NONE;

		if (scan_result.wpa2_entp) {
		}
		if (scan_result.wpa2) {
			res.security = WIFI_SECURITY_TYPE_PSK;
		}
#ifdef CONFIG_WPA_SUPP
		if (scan_result.wpa2_sha256) {
			res.security = WIFI_SECURITY_TYPE_PSK_SHA256;
		}
#endif
		if (scan_result.wpa3_sae) {
			res.security = WIFI_SECURITY_TYPE_SAE;
		}

		if (g_mlan.scan_cb) {
			g_mlan.scan_cb(g_mlan.netif, 0, &res);

			/* ensure notifications get delivered */
			k_yield();
		}
	}

out:
	/* report end of scan event */
	g_mlan.scan_cb(g_mlan.netif, 0, NULL);
	g_mlan.scan_cb = NULL;

	k_event_set(&s_wplSyncEvent, EVENT_BIT(EVENT_SCAN_DONE));

	return WM_SUCCESS;
}

static int WPL_Scan(const struct device *dev, scan_result_cb_t cb)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;
	interface_t *if_handle = (interface_t *)dev->data;

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	if (if_handle->scan_cb != NULL) {
		LOG_INF("Scan callback in progress");
		return -EINPROGRESS;
	}

	if_handle->scan_cb = cb;

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_scan(WLP_process_results);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_SCAN_GROUP, true,
				       WPL_SYNC_TIMEOUT_MS);
		if (syncBit & EVENT_BIT(EVENT_SCAN_DONE)) {
			status = WPLRET_SUCCESS;
		} else {
			status = WPLRET_TIMEOUT;
		}
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to start Wi-Fi scanning");
		return -EAGAIN;
	}

	return 0;
}

static int WPL_Disconnect(const struct device *dev);

static int WPL_Connect(const struct device *dev, struct wifi_connect_req_params *params)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;
	interface_t *if_handle = (interface_t *)dev->data;

	if ((s_wplState != WPL_STARTED) || (s_wplStaConnected != false)) {
		status = WPLRET_NOT_READY;
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, -1);
		return -EALREADY;
	}

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, -1);
		return -EIO;
	}

	if (status == WPLRET_SUCCESS) {
		if ((params->ssid_length == 0) || (params->ssid_length > IEEEtypes_SSID_SIZE)) {
			status = WPLRET_BAD_PARAM;
		}
	}

	if (status == WPLRET_SUCCESS) {
		wlan_remove_network(sta_network.name);

		wlan_initialize_sta_network(&sta_network);

		memcpy(sta_network.ssid, params->ssid, params->ssid_length);
		sta_network.ssid_specific = 1;

		sta_network.channel = params->channel;

		if (params->mfp == WIFI_MFP_REQUIRED) {
			sta_network.security.mfpc = true;
			sta_network.security.mfpr = true;
		} else if (params->mfp == WIFI_MFP_OPTIONAL) {
			sta_network.security.mfpc = true;
			sta_network.security.mfpr = false;
		}

		if (params->security == WIFI_SECURITY_TYPE_NONE) {
			sta_network.security.type = WLAN_SECURITY_NONE;
		} else if (params->security == WIFI_SECURITY_TYPE_PSK) {
			sta_network.security.type = WLAN_SECURITY_WPA2;
			sta_network.security.psk_len = params->psk_length;
			strncpy(sta_network.security.psk, params->psk, params->psk_length);
		}
#ifdef CONFIG_WPA_SUPP
		else if (params->security == WIFI_SECURITY_TYPE_PSK_SHA256) {
			sta_network.security.type = WLAN_SECURITY_WPA2_SHA256;
			sta_network.security.psk_len = params->psk_length;
			strncpy(sta_network.security.psk, params->psk, params->psk_length);
		}
#endif
		else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			sta_network.security.type = WLAN_SECURITY_WPA3_SAE;
			sta_network.security.password_len = params->sae_password_length;
			strncpy(sta_network.security.password, params->sae_password,
				params->sae_password_length);
		} else {
			status = WPLRET_BAD_PARAM;
		}
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_add_network(&sta_network);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		k_event_clear(&s_wplSyncEvent, WPL_SYNC_CONNECT_GROUP);

		ret = wlan_connect(sta_network.name);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_CONNECT_GROUP, true,
				       WPL_SYNC_TIMEOUT_MS);
		if (syncBit & EVENT_BIT(WLAN_REASON_SUCCESS)) {
			status = WPLRET_SUCCESS;
		} else if (syncBit & EVENT_BIT(WLAN_REASON_CONNECT_FAILED)) {
			status = WPLRET_FAIL;
		} else if (syncBit & EVENT_BIT(WLAN_REASON_NETWORK_NOT_FOUND)) {
			status = WPLRET_NOT_FOUND;
		} else if (syncBit & EVENT_BIT(WLAN_REASON_NETWORK_AUTH_FAILED)) {
			status = WPLRET_AUTH_FAILED;
		} else if (syncBit & EVENT_BIT(WLAN_REASON_ADDRESS_FAILED)) {
			status = WPLRET_ADDR_FAILED;
		} else {
			status = WPLRET_TIMEOUT;
		}

		if (status != WPLRET_SUCCESS) {
			/* Abort the next connection attempt */
			WPL_Disconnect(dev);
		}
	}

	if (status == WPLRET_SUCCESS) {
		s_wplStaConnected = true;
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, 0);
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to connect to Wi-Fi access point");
		return -EAGAIN;
	}

	return 0;
}

static int WPL_Disconnect(const struct device *dev)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	uint32_t syncBit;
	interface_t *if_handle = (interface_t *)dev->data;

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;
	wlan_get_connection_state(&connection_state);
	if (connection_state == WLAN_DISCONNECTED) {
		s_wplStaConnected = false;
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return WPLRET_SUCCESS;
	}

	if (status == WPLRET_SUCCESS) {
		k_event_clear(&s_wplSyncEvent, WPL_SYNC_DISCONNECT_GROUP);

		ret = wlan_disconnect();
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status == WPLRET_SUCCESS) {
		syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_DISCONNECT_GROUP, true,
				       WPL_SYNC_TIMEOUT_MS);
		if (syncBit & EVENT_BIT(WLAN_REASON_USER_DISCONNECT)) {
			status = WPLRET_SUCCESS;
		} else {
			status = WPLRET_TIMEOUT;
		}
	}

	s_wplStaConnected = false;

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to disconnect from AP");
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return -EAGAIN;
	}

	wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, 0);

	return 0;
}

int WPL_GetIP(char *ip, int client)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;
	struct wlan_ip_config addr;

	if (ip == NULL) {
		status = WPLRET_FAIL;
	}

	if (status == WPLRET_SUCCESS) {
		if (client) {
			ret = wlan_get_address(&addr);
		} else {
			ret = wlan_get_uap_address(&addr);
		}

		if (ret == WM_SUCCESS) {
			net_inet_ntoa(addr.ipv4.address, ip);
		} else {
			status = WPLRET_FAIL;
		}
	}

	if (status != WPLRET_SUCCESS) {
		return -1;
	}

	return 0;
}

static inline enum wifi_security_type WPL_key_mgmt_to_zephyr(enum wlan_security_type type)
{
	switch (type) {
	case WLAN_SECURITY_NONE:
		return WIFI_SECURITY_TYPE_NONE;
	case WLAN_SECURITY_WPA2:
		return WIFI_SECURITY_TYPE_PSK;
#ifdef CONFIG_WPA_SUPP
	case WLAN_SECURITY_WPA2_SHA256:
		return WIFI_SECURITY_TYPE_PSK_SHA256;
#endif
	case WLAN_SECURITY_WPA3_SAE:
		return WIFI_SECURITY_TYPE_SAE;
	default:
		return WIFI_SECURITY_TYPE_UNKNOWN;
	}
}

static int WPL_Status(const struct device *dev, struct wifi_iface_status *status)
{
	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;
	wlan_get_connection_state(&connection_state);
	struct wlan_network *network =
		(struct wlan_network *)os_mem_alloc(sizeof(struct wlan_network));
	if (!network) {
		LOG_ERR("Error: unable to malloc memory");
		return -1;
	}

	if (s_wplState != WPL_STARTED) {
		status->state = WIFI_STATE_INTERFACE_DISABLED;
		return WPLRET_SUCCESS;
	}

	if (connection_state == WLAN_DISCONNECTED) {
		status->state = WIFI_STATE_DISCONNECTED;
	} else if (connection_state == WLAN_SCANNING) {
		status->state = WIFI_STATE_SCANNING;
	} else if (connection_state == WLAN_ASSOCIATING) {
		status->state = WIFI_STATE_ASSOCIATING;
	} else if (connection_state == WLAN_ASSOCIATED) {
		status->state = WIFI_STATE_ASSOCIATED;
	} else if (connection_state == WLAN_CONNECTED) {
		status->state = WIFI_STATE_COMPLETED;

		if (!wlan_get_current_network(network)) {
			strncpy(status->ssid, network->ssid, strlen(network->ssid));
			status->ssid_len = strlen(network->ssid);

			memcpy(status->bssid, network->bssid, WIFI_MAC_ADDR_LEN);

			status->rssi = network->rssi;

			status->channel = network->channel;

			status->iface_mode = WIFI_MODE_INFRA;

			status->band = network->channel > 14 ? WIFI_FREQ_BAND_5_GHZ
							     : WIFI_FREQ_BAND_2_4_GHZ;
			status->security = WPL_key_mgmt_to_zephyr(network->security.type);
			status->mfp = network->security.mfpr   ? WIFI_MFP_REQUIRED
				      : network->security.mfpc ? WIFI_MFP_OPTIONAL
							       : 0;
		}
	}

	return 0;
}

K_THREAD_STACK_DEFINE(net_wifi_init_stack, CONFIG_WIFI_INIT_STACK_SIZE);
struct k_thread net_wifi_thread;

#ifdef RW610
extern void WL_MCI_WAKEUP0_DriverIRQHandler(void);
#endif

/* IW416 network init thread */
static void wifi_drv_init(void *dev, void *arg2, void *arg3)
{
	int ret;

#ifdef RW610
	IRQ_CONNECT(72, 1, WL_MCI_WAKEUP0_DriverIRQHandler, 0, 0);
	irq_enable(72);
#endif

	/* Initialize the wifi subsystem */
	ret = WPL_Init();
	if (ret) {
		LOG_ERR("wlan initialization failed");
		return;
	}
	ret = WPL_Start(LinkStatusChangeCallback);
	if (ret) {
		LOG_ERR("could not start wlan threads");
		return;
	}
}

static int wifi_net_init(const struct device *dev)
{
	return 0;
}

static int wifi_net_init_thread(const struct device *dev)
{
	g_mlan.state.interface = WLAN_BSS_TYPE_STA;
	g_uap.state.interface = WLAN_BSS_TYPE_UAP;

	/* kickoff init thread to avoid stack overflow */
	k_thread_create(&net_wifi_thread, net_wifi_init_stack,
			K_THREAD_STACK_SIZEOF(net_wifi_init_stack), wifi_drv_init, (void *)dev,
			NULL, NULL, 0, 0, K_NO_WAIT);

	return 0;
}

static void wifi_net_iface_init(struct net_if *iface)
{
	static int init_done = 0;
	const struct device *dev = net_if_get_device(iface);
	interface_t *intf = dev->data;

	intf->netif = iface;

	if (!init_done) {
		wifi_net_init_thread(dev);
		init_done = 1;
	}

	net_if_flag_set(iface, NET_IF_NO_AUTO_START);
}

#ifdef CONFIG_NET_STATISTICS_WIFI
int z_wpa_supplicant_get_stats(const struct device *dev, struct net_stats_wifi *stats)
{
	LOG_INF("%s", __func__);

	return 0;
}
#endif

static int z_wpa_supplicant_set_power_save(const struct device *dev, struct wifi_ps_params *params)
{
	LOG_INF("%s", __func__);

	return 0;
}

static int z_wpa_supplicant_set_twt(const struct device *dev, struct wifi_twt_params *params)
{
	LOG_INF("%s", __func__);

	return 0;
}

int z_wpa_supplicant_get_power_save_config(const struct device *dev, struct wifi_ps_config *config)
{
	LOG_INF("%s", __func__);

	return 0;
}

int z_wpa_supplicant_reg_domain(const struct device *dev, struct wifi_reg_domain *reg_domain)
{
	LOG_INF("%s", __func__);

	return 0;
}

static const struct wifi_mgmt_ops nxp_wifi_mgmt = {
	.scan = WPL_Scan,
	.connect = WPL_Connect,
	.disconnect = WPL_Disconnect,
	.ap_enable = WPL_Start_AP,
	.ap_disable = WPL_Stop_AP,
	.iface_status = WPL_Status,
#ifdef CONFIG_NET_STATISTICS_WIFI
	.get_stats = z_wpa_supplicant_get_stats,
#endif
	.set_power_save = z_wpa_supplicant_set_power_save,
	.set_twt = z_wpa_supplicant_set_twt,
	.get_power_save_config = z_wpa_supplicant_get_power_save_config,
	.reg_domain = z_wpa_supplicant_reg_domain,
};

static const struct net_wifi_mgmt_offload wifi_netif_apis = {
	.wifi_iface.iface_api.init = wifi_net_iface_init,
	.wifi_iface.send = low_level_output,
	.wifi_mgmt_api = &nxp_wifi_mgmt,
};

NET_DEVICE_INIT_INSTANCE(wifi_nxp_sta, "ml", 0, wifi_net_init, NULL, &g_mlan,
#ifdef CONFIG_WPA_SUPP
			 &wpa_supp_ops,
#else
			 NULL,
#endif
			 CONFIG_ETH_INIT_PRIORITY, &wifi_netif_apis, ETHERNET_L2,
			 NET_L2_GET_CTX_TYPE(ETHERNET_L2), NET_ETH_MTU);

NET_DEVICE_INIT_INSTANCE(wifi_nxp_uap, "ua", 1, wifi_net_init, NULL, &g_uap,
#ifdef CONFIG_WPA_SUPP
			 &wpa_supp_ops,
#else
			 NULL,
#endif
			 CONFIG_ETH_INIT_PRIORITY, &wifi_netif_apis, ETHERNET_L2,
			 NET_L2_GET_CTX_TYPE(ETHERNET_L2), NET_ETH_MTU);
