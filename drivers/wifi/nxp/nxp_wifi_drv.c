/**
 * Copyright 2023 NXP
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file nxp_wifi_drv.c
 * Shim layer between wifi driver connection manager and zephyr
 * Wi-Fi L2 layer
 */

#define DT_DRV_COMPAT nxp_wifi

#include <zephyr/net/ethernet.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/device.h>
#include <soc.h>
#include <ethernet/eth_stats.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(nxp_wifi, CONFIG_WIFI_LOG_LEVEL);

#include "nxp_wifi_drv.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
static nxp_wifi_state_t s_nxp_wifi_State = NXP_WIFI_NOT_INITIALIZED;
static bool s_nxp_wifi_StaConnected;
static bool s_nxp_wifi_UapActivated;
static struct k_event s_nxp_wifi_SyncEvent;

static void nxp_wifi_task(void *, void *, void *);

K_THREAD_STACK_DEFINE(nxp_wifi_stack, CONFIG_NXP_WIFI_TASK_STACK_SIZE);
static struct k_thread nxp_wifi_thread;

static struct nxp_wifi_dev nxp_wifi0; /* static instance */

static struct wlan_network nxp_wlan_network;

static char uap_ssid[IEEEtypes_SSID_SIZE + 1];

extern interface_t g_mlan;
extern interface_t g_uap;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
#ifdef CONFIG_NXP_WIFI_STA_AUTO
static void nxp_wifi_auto_connect(void);
#endif

/* Callback Function passed to WLAN Connection Manager. The callback function
 * gets called when there are WLAN Events that need to be handled by the
 * application.
 */
static int nxp_wifi_wlan_event_callback(enum wlan_event_reason reason, void *data)
{
	int ret;
	struct wlan_ip_config addr;
	char ssid[IEEEtypes_SSID_SIZE + 1];
	char ip[16];
	static int auth_fail;
	wlan_uap_client_disassoc_t *disassoc_resp = data;

	LOG_DBG("nxp_wifi: WLAN: received event %d", reason);

	if (s_nxp_wifi_State >= NXP_WIFI_INITIALIZED) {
		/* Do not replace the current set of events  */
		k_event_post(&s_nxp_wifi_SyncEvent, NXP_WIFI_EVENT_BIT(reason));
	}

	switch (reason) {
	case WLAN_REASON_INITIALIZED:
		LOG_DBG("nxp_wifi: WLAN initialized");

#ifdef CONFIG_NET_INTERFACE_NAME
		ret = net_if_set_name(g_mlan.netif, "ml");
		if (ret < 0) {
			LOG_ERR("Failed to set STA nxp_wlan_network interface name");
			return 0;
		}

		ret = net_if_set_name(g_uap.netif, "ua");
		if (ret < 0) {
			LOG_ERR("Failed to set uAP nxp_wlan_network interface name");
			return 0;
		}
#endif

#ifdef CONFIG_NXP_WIFI_STA_AUTO
		if (IS_ENABLED(CONFIG_NXP_WIFI_STA_AUTO)) {
			nxp_wifi_auto_connect();
		}
#endif
		break;
	case WLAN_REASON_INITIALIZATION_FAILED:
		LOG_ERR("nxp_wifi: WLAN: initialization failed");
		break;
	case WLAN_REASON_AUTH_SUCCESS:
		net_eth_carrier_on(g_mlan.netif);
		LOG_DBG("nxp_wifi: WLAN: authenticated to nxp_wlan_network");
		break;
	case WLAN_REASON_SUCCESS:
		LOG_DBG("nxp_wifi: WLAN: connected to nxp_wlan_network");
		ret = wlan_get_address(&addr);
		if (ret != WM_SUCCESS) {
			LOG_ERR("failed to get IP address");
			return 0;
		}

		net_inet_ntoa(addr.ipv4.address, ip);

		ret = wlan_get_current_network_ssid(ssid);
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to get External AP nxp_wlan_network ssid");
			return 0;
		}

		LOG_DBG("Connected to following BSS:");
		LOG_DBG("SSID = [%s]", ssid);
		if (addr.ipv4.address != 0U) {
			LOG_DBG("IPv4 Address: [%s]", ip);
		}
#ifdef CONFIG_IPV6
		int i;

		for (i = 0; i < CONFIG_MAX_IPV6_ADDRESSES; i++) {
			if ((addr.ipv6[i].addr_state == NET_ADDR_TENTATIVE) ||
			    (addr.ipv6[i].addr_state == NET_ADDR_PREFERRED)) {
				LOG_DBG("IPv6 Address: %-13s:\t%s (%s)",
					ipv6_addr_type_to_desc(
						(struct net_ipv6_config *)&addr.ipv6[i]),
					ipv6_addr_addr_to_desc(
						(struct net_ipv6_config *)&addr.ipv6[i]),
					ipv6_addr_state_to_desc(addr.ipv6[i].addr_state));
			}
		}
		LOG_DBG("");
#endif
		auth_fail = 0;
		s_nxp_wifi_StaConnected = true;
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, 0);
		break;
	case WLAN_REASON_CONNECT_FAILED:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("nxp_wifi: WLAN: connect failed");
		break;
	case WLAN_REASON_NETWORK_NOT_FOUND:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("nxp_wifi: WLAN: nxp_wlan_network not found");
		break;
	case WLAN_REASON_NETWORK_AUTH_FAILED:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("nxp_wifi: WLAN: nxp_wlan_network authentication failed");
		auth_fail++;
		if (auth_fail >= 3) {
			LOG_WRN("Authentication Failed. Disconnecting ... ");
			wlan_disconnect();
			auth_fail = 0;
		}
		break;
	case WLAN_REASON_ADDRESS_SUCCESS:
		LOG_DBG("nxp_wlan_network mgr: DHCP new lease");
		break;
	case WLAN_REASON_ADDRESS_FAILED:
		LOG_WRN("nxp_wifi: failed to obtain an IP address");
		break;
	case WLAN_REASON_USER_DISCONNECT:
		net_eth_carrier_off(g_mlan.netif);
		LOG_DBG("nxp_wifi: disconnected");
		auth_fail = 0;
		s_nxp_wifi_StaConnected = false;
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, 0);
		break;
	case WLAN_REASON_LINK_LOST:
		net_eth_carrier_off(g_mlan.netif);
		LOG_WRN("nxp_wifi: WLAN: link lost");
		break;
	case WLAN_REASON_CHAN_SWITCH:
		LOG_DBG("nxp_wifi: WLAN: channel switch");
		break;
	case WLAN_REASON_UAP_SUCCESS:
		net_eth_carrier_on(g_uap.netif);
		LOG_DBG("nxp_wifi: WLAN: UAP Started");

		ret = wlan_get_current_uap_network_ssid(uap_ssid);
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to get Soft AP nxp_wlan_network ssid");
			return 0;
		}

		LOG_DBG("Soft AP \"%s\" started successfully", uap_ssid);

		if (dhcp_server_start(net_get_uap_handle())) {
			LOG_ERR("Error in starting dhcp server");
			return 0;
		}

		LOG_DBG("DHCP Server started successfully");
		s_nxp_wifi_UapActivated = true;
		break;
	case WLAN_REASON_UAP_CLIENT_ASSOC:
		LOG_DBG("nxp_wifi: WLAN: UAP a Client Associated");
		LOG_DBG("Client => ");
		print_mac((const char *)data);
		LOG_DBG("Associated with Soft AP");
		break;
	case WLAN_REASON_UAP_CLIENT_CONN:
		LOG_DBG("nxp_wifi: WLAN: UAP a Client Connected");
		LOG_DBG("Client => ");
		print_mac((const char *)data);
		LOG_DBG("Connected with Soft AP");
		break;
	case WLAN_REASON_UAP_CLIENT_DISSOC:
		LOG_DBG("nxp_wifi: WLAN: UAP a Client Dissociated:");
		LOG_DBG(" Client MAC => ");
		print_mac((const char *)(disassoc_resp->sta_addr));
		LOG_DBG(" Reason code => ");
		LOG_DBG("%d", disassoc_resp->reason_code);
		break;
	case WLAN_REASON_UAP_STOPPED:
		net_eth_carrier_off(g_uap.netif);
		LOG_DBG("nxp_wifi: WLAN: UAP Stopped");

		LOG_DBG("Soft AP \"%s\" stopped successfully", uap_ssid);

		dhcp_server_stop();

		LOG_DBG("DHCP Server stopped successfully");
		s_nxp_wifi_UapActivated = false;
		break;
	case WLAN_REASON_PS_ENTER:
		LOG_DBG("nxp_wifi: WLAN: PS_ENTER");
		break;
	case WLAN_REASON_PS_EXIT:
		LOG_DBG("nxp_wifi: WLAN: PS EXIT");
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
		LOG_WRN("nxp_wifi: WLAN: Unknown Event: %d", reason);
	}
	return 0;
}

static int nxp_wifi_wlan_init(void)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;

	if (s_nxp_wifi_State != NXP_WIFI_NOT_INITIALIZED) {
		status = NXP_WIFI_RET_FAIL;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		k_event_init(&s_nxp_wifi_SyncEvent);
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_init(wlan_fw_bin, wlan_fw_bin_len);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		s_nxp_wifi_State = NXP_WIFI_INITIALIZED;
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		return -1;
	}

	return 0;
}

static int nxp_wifi_wlan_start(void)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	uint32_t syncBit;

	if (s_nxp_wifi_State != NXP_WIFI_INITIALIZED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_INIT_GROUP);

		ret = wlan_start(nxp_wifi_wlan_event_callback);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		syncBit = k_event_wait(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_INIT_GROUP, false,
				       NXP_WIFI_SYNC_TIMEOUT_MS);
		k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_INIT_GROUP);
		if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_INITIALIZED)) {
			status = NXP_WIFI_RET_SUCCESS;
		} else if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_INITIALIZATION_FAILED)) {
			status = NXP_WIFI_RET_FAIL;
		} else {
			status = NXP_WIFI_RET_TIMEOUT;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		s_nxp_wifi_State = NXP_WIFI_STARTED;
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		return -1;
	}

	return 0;
}

#ifdef CONFIG_NXP_WIFI_SOFTAP_SUPPORT

static int nxp_wifi_start_ap(const struct device *dev, struct wifi_connect_req_params *params)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	interface_t *if_handle = (interface_t *)&g_uap;

	if (if_handle->state.interface != WLAN_BSS_TYPE_UAP) {
		LOG_ERR("Wi-Fi not in uAP mode");
		return -EIO;
	}

	if ((s_nxp_wifi_State != NXP_WIFI_STARTED) || (s_nxp_wifi_UapActivated != false)) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		if ((params->ssid_length == 0) || (params->ssid_length > IEEEtypes_SSID_SIZE)) {
			status = NXP_WIFI_RET_BAD_PARAM;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		wlan_remove_network(nxp_wlan_network.name);

		wlan_initialize_uap_network(&nxp_wlan_network);

		memcpy(nxp_wlan_network.name, NXP_WIFI_UAP_NETWORK_NAME,
		       strlen(NXP_WIFI_UAP_NETWORK_NAME));

		memcpy(nxp_wlan_network.ssid, params->ssid, params->ssid_length);

		if (params->channel == WIFI_CHANNEL_ANY) {
			nxp_wlan_network.channel = 0;
		} else {
			nxp_wlan_network.channel = params->channel;
		}

		if (params->mfp == WIFI_MFP_REQUIRED) {
			nxp_wlan_network.security.mfpc = true;
			nxp_wlan_network.security.mfpr = true;
		} else if (params->mfp == WIFI_MFP_OPTIONAL) {
			nxp_wlan_network.security.mfpc = true;
			nxp_wlan_network.security.mfpr = false;
		}

		if (params->security == WIFI_SECURITY_TYPE_NONE) {
			nxp_wlan_network.security.type = WLAN_SECURITY_NONE;
		} else if (params->security == WIFI_SECURITY_TYPE_PSK) {
			nxp_wlan_network.security.type = WLAN_SECURITY_WPA2;
			nxp_wlan_network.security.psk_len = params->psk_length;
			strncpy(nxp_wlan_network.security.psk, params->psk, params->psk_length);
		} else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			nxp_wlan_network.security.type = WLAN_SECURITY_WPA3_SAE;
			nxp_wlan_network.security.password_len = params->psk_length;
			strncpy(nxp_wlan_network.security.password, params->psk,
				params->psk_length);
		} else {
			status = NXP_WIFI_RET_BAD_PARAM;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_add_network(&nxp_wlan_network);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_start_network(nxp_wlan_network.name);
		if (ret != WM_SUCCESS) {
			wlan_remove_network(nxp_wlan_network.name);
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to enable Wi-Fi AP mode");
		return -EAGAIN;
	}

	return 0;
}

static int nxp_wifi_stop_ap(const struct device *dev)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	interface_t *if_handle = (interface_t *)&g_uap;

	if (if_handle->state.interface != WLAN_BSS_TYPE_UAP) {
		LOG_ERR("Wi-Fi not in uAP mode");
		return -EIO;
	}

	if ((s_nxp_wifi_State != NXP_WIFI_STARTED) || (s_nxp_wifi_UapActivated != true)) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_stop_network(NXP_WIFI_UAP_NETWORK_NAME);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to disable Wi-Fi AP mode");
		return -EAGAIN;
	}

	return 0;
}

#endif

static int nxp_wifi_process_results(unsigned int count)
{
	struct wlan_scan_result scan_result = {0};
	struct wifi_scan_result res = {0};

	if (!count) {
		LOG_DBG("No Wi-Fi AP found");
		goto out;
	}

	if (g_mlan.max_bss_cnt) {
		count = g_mlan.max_bss_cnt > count ? count : g_mlan.max_bss_cnt;
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
			res.security = WIFI_SECURITY_TYPE_EAP;
		}
		if (scan_result.wpa2) {
			res.security = WIFI_SECURITY_TYPE_PSK;
		}
		if (scan_result.wpa3_sae) {
			res.security = WIFI_SECURITY_TYPE_SAE;
		}

		if (scan_result.ap_mfpr) {
			res.mfp = WIFI_MFP_REQUIRED;
		} else if (scan_result.ap_mfpc) {
			res.mfp = WIFI_MFP_OPTIONAL;
		}

		if (g_mlan.scan_cb) {
			g_mlan.scan_cb(g_mlan.netif, 0, &res);

			/* ensure notifications get delivered */
			k_yield();
		}
	}

out:
	/* report end of scan event */
	if (g_mlan.scan_cb) {
		g_mlan.scan_cb(g_mlan.netif, 0, NULL);
	}

	g_mlan.scan_cb = NULL;

	return WM_SUCCESS;
}

static int nxp_wifi_scan(const struct device *dev, struct wifi_scan_params *params,
			 scan_result_cb_t cb)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	interface_t *if_handle = (interface_t *)dev->data;
	wlan_scan_params_v2_t wlan_scan_params_v2 = {0};
	uint8_t i = 0;

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	if (params->bands & (1 << WIFI_FREQ_BAND_6_GHZ)) {
		LOG_ERR("Wi-Fi band 6 GHz not supported");
		return -EIO;
	}

	g_mlan.scan_cb = cb;
	if_handle->scan_cb = cb;
	if_handle->max_bss_cnt = params->max_bss_cnt;

#if (CONFIG_WIFI_MGMT_SCAN_SSID_FILT_MAX > 0)

#ifdef CONFIG_COMBO_SCAN
	if (params->ssids[0]) {
		strcpy(wlan_scan_params_v2.ssid[0], params->ssids[0]);
	}

#if (CONFIG_WIFI_MGMT_SCAN_SSID_FILT_MAX > 1)
	if (params->ssids[1]) {
		strcpy(wlan_scan_params_v2.ssid[1], params->ssids[1]);
	}
#endif

#else
	if (params->ssids[0]) {
		strcpy(wlan_scan_params_v2.ssid, params->ssids[0]);
	}
#endif
#endif

#ifdef CONFIG_WIFI_MGMT_SCAN_CHAN_MAX_MANUAL

	for (i = 0; i < WIFI_MGMT_SCAN_CHAN_MAX_MANUAL && params->band_chan[i].channel; i++) {
		if (params->scan_type == WIFI_SCAN_TYPE_ACTIVE) {
			wlan_scan_params_v2.chan_list[i].scan_type = MLAN_SCAN_TYPE_ACTIVE;
			wlan_scan_params_v2.chan_list[i].scan_time = params->dwell_time_active;
		} else {
			wlan_scan_params_v2.chan_list[i].scan_type = MLAN_SCAN_TYPE_PASSIVE;
			wlan_scan_params_v2.chan_list[i].scan_time = params->dwell_time_passive;
		}

		wlan_scan_params_v2.chan_list[i].chan_number =
			(uint8_t)(params->band_chan[i].channel);
	}
#endif

	wlan_scan_params_v2.num_channels = i;

	if (params->bands & (1 << WIFI_FREQ_BAND_2_4_GHZ)) {
		wlan_scan_params_v2.chan_list[0].radio_type = 0 | BAND_SPECIFIED;
	}
#ifdef CONFIG_5GHz_SUPPORT
	if (params->bands & (1 << WIFI_FREQ_BAND_5_GHZ)) {
		if (wlan_scan_params_v2.chan_list[0].radio_type & BAND_SPECIFIED) {
			wlan_scan_params_v2.chan_list[0].radio_type = 0;
		} else {
			wlan_scan_params_v2.chan_list[0].radio_type = 1 | BAND_SPECIFIED;
		}
	}
#else
	if (params->bands & (1 << WIFI_FREQ_BAND_5_GHZ)) {
		LOG_ERR("Wi-Fi band 6 5Hz not supported");
		return -EIO;
	}
#endif

	wlan_scan_params_v2.cb = nxp_wifi_process_results;

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_scan_with_opt(wlan_scan_params_v2);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to start Wi-Fi scanning");
		return -EAGAIN;
	}

	return 0;
}

static int nxp_wifi_version(const struct device *dev, struct wifi_version *params)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to get Wi-Fi driver/firmware version");
		return -EAGAIN;
	}

	params->drv_version = WLAN_DRV_VERSION;

	params->fw_version = wlan_get_firmware_version_ext();

	return 0;
}

static int nxp_wifi_ap_bandwidth(const struct device *dev, struct wifi_ap_params *params)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret = WM_SUCCESS;
	interface_t *if_handle = (interface_t *)dev->data;

	if (if_handle->state.interface != WLAN_BSS_TYPE_UAP) {
		LOG_ERR("Wi-Fi not in uAP mode");
		return -EIO;
	}

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {

		if (params->oper == WIFI_MGMT_SET) {

			ret = wlan_uap_set_bandwidth(params->bandwidth);

			if (ret != WM_SUCCESS) {
				status = NXP_WIFI_RET_FAIL;
			}
		} else {
			ret = wlan_uap_get_bandwidth(&params->bandwidth);

			if (ret != WM_SUCCESS) {
				status = NXP_WIFI_RET_FAIL;
			}
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to get/set Wi-Fi AP bandwidth");
		return -EAGAIN;
	}

	return 0;
}

static int nxp_wifi_connect(const struct device *dev, struct wifi_connect_req_params *params)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	interface_t *if_handle = (interface_t *)dev->data;

	if ((s_nxp_wifi_State != NXP_WIFI_STARTED) || (s_nxp_wifi_StaConnected != false)) {
		status = NXP_WIFI_RET_NOT_READY;
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, -1);
		return -EALREADY;
	}

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, -1);
		return -EIO;
	}

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		if ((params->ssid_length == 0) || (params->ssid_length > IEEEtypes_SSID_SIZE)) {
			status = NXP_WIFI_RET_BAD_PARAM;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		wlan_disconnect();

		wlan_remove_network(nxp_wlan_network.name);

		wlan_initialize_sta_network(&nxp_wlan_network);

		memcpy(nxp_wlan_network.name, NXP_WIFI_STA_NETWORK_NAME,
		       strlen(NXP_WIFI_STA_NETWORK_NAME));

		memcpy(nxp_wlan_network.ssid, params->ssid, params->ssid_length);

		nxp_wlan_network.ssid_specific = 1;

		if (params->channel == WIFI_CHANNEL_ANY) {
			nxp_wlan_network.channel = 0;
		} else {
			nxp_wlan_network.channel = params->channel;
		}

#ifdef CONFIG_NXP_WIFI_IP_STATIC
		nxp_wlan_network.ip.ipv4.addr_type = NET_ADDR_TYPE_STATIC;

		nxp_wlan_network.ip.ipv4.address = net_inet_aton(CONFIG_NXP_WIFI_IP_ADDRESS);
		nxp_wlan_network.ip.ipv4.netmask = net_inet_aton(CONFIG_NXP_WIFI_IP_MASK);
		nxp_wlan_network.ip.ipv4.gw = net_inet_aton(CONFIG_NXP_WIFI_IP_GATEWAY);
#endif

		if (params->mfp == WIFI_MFP_REQUIRED) {
			nxp_wlan_network.security.mfpc = true;
			nxp_wlan_network.security.mfpr = true;
		} else if (params->mfp == WIFI_MFP_OPTIONAL) {
			nxp_wlan_network.security.mfpc = true;
			nxp_wlan_network.security.mfpr = false;
		}

		if (params->security == WIFI_SECURITY_TYPE_NONE) {
			nxp_wlan_network.security.type = WLAN_SECURITY_NONE;
		} else if (params->security == WIFI_SECURITY_TYPE_PSK) {
			nxp_wlan_network.security.type = WLAN_SECURITY_WPA2;
			nxp_wlan_network.security.psk_len = params->psk_length;
			strncpy(nxp_wlan_network.security.psk, params->psk, params->psk_length);
		} else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			nxp_wlan_network.security.type = WLAN_SECURITY_WPA3_SAE;
			nxp_wlan_network.security.password_len = params->psk_length;
			strncpy(nxp_wlan_network.security.password, params->psk,
				params->psk_length);
		} else {
			status = NXP_WIFI_RET_BAD_PARAM;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_add_network(&nxp_wlan_network);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_connect(nxp_wlan_network.name);
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to connect to Wi-Fi access point");
		return -EAGAIN;
	}

	return 0;
}

static int nxp_wifi_disconnect(const struct device *dev)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret;
	interface_t *if_handle = (interface_t *)dev->data;
	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	wlan_get_connection_state(&connection_state);
	if (connection_state == WLAN_DISCONNECTED) {
		s_nxp_wifi_StaConnected = false;
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return NXP_WIFI_RET_SUCCESS;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		ret = wlan_disconnect();
		if (ret != WM_SUCCESS) {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to disconnect from AP");
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return -EAGAIN;
	}

	wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, 0);

	return 0;
}

static inline enum wifi_security_type nxp_wifi_security_type(enum wlan_security_type type)
{
	switch (type) {
	case WLAN_SECURITY_NONE:
		return WIFI_SECURITY_TYPE_NONE;
	case WLAN_SECURITY_WPA2:
		return WIFI_SECURITY_TYPE_PSK;
	case WLAN_SECURITY_WPA3_SAE:
		return WIFI_SECURITY_TYPE_SAE;
	default:
		return WIFI_SECURITY_TYPE_UNKNOWN;
	}
}

static int nxp_wifi_uap_status(const struct device *dev, struct wifi_iface_status *status)
{
	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;
	interface_t *if_handle = (interface_t *)&g_uap;

	wlan_get_uap_connection_state(&connection_state);

	if (connection_state == WLAN_UAP_STARTED) {

		if (!wlan_get_current_uap_network(&nxp_wlan_network)) {
			strcpy(status->ssid, nxp_wlan_network.ssid);
			status->ssid_len = strlen(nxp_wlan_network.ssid);

			memcpy(status->bssid, nxp_wlan_network.bssid, WIFI_MAC_ADDR_LEN);

			status->rssi = nxp_wlan_network.rssi;

			status->channel = nxp_wlan_network.channel;

			status->beacon_interval = nxp_wlan_network.beacon_period;

			status->dtim_period = nxp_wlan_network.dtim_period;

			if (if_handle->state.interface == WLAN_BSS_TYPE_STA) {
				status->iface_mode = WIFI_MODE_INFRA;
			}
#ifdef CONFIG_NXP_WIFI_SOFTAP_SUPPORT
			else if (if_handle->state.interface == WLAN_BSS_TYPE_UAP) {
				status->iface_mode = WIFI_MODE_AP;
			}
#endif

#ifdef CONFIG_11AX
			if (nxp_wlan_network.dot11ax) {
				status->link_mode = WIFI_6;
			}
#endif
#ifdef CONFIG_11AC
			else if (nxp_wlan_network.dot11ac) {
				status->link_mode = WIFI_5;
			}
#endif
			else if (nxp_wlan_network.dot11n) {
				status->link_mode = WIFI_4;
			} else {
				status->link_mode = WIFI_3;
			}

			status->band = nxp_wlan_network.channel > 14 ? WIFI_FREQ_BAND_5_GHZ
								     : WIFI_FREQ_BAND_2_4_GHZ;
			status->security = nxp_wifi_security_type(nxp_wlan_network.security.type);
			status->mfp = nxp_wlan_network.security.mfpr   ? WIFI_MFP_REQUIRED
				      : nxp_wlan_network.security.mfpc ? WIFI_MFP_OPTIONAL
								       : 0;
		}
	}

	return 0;
}

static int nxp_wifi_status(const struct device *dev, struct wifi_iface_status *status)
{
	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;
	interface_t *if_handle = (interface_t *)dev->data;

	wlan_get_connection_state(&connection_state);

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status->state = WIFI_STATE_INTERFACE_DISABLED;

		return 0;
	}

	if (connection_state == WLAN_DISCONNECTED) {
		status->state = WIFI_STATE_DISCONNECTED;
		return nxp_wifi_uap_status(dev, status);
	} else if (connection_state == WLAN_SCANNING) {
		status->state = WIFI_STATE_SCANNING;
	} else if (connection_state == WLAN_ASSOCIATING) {
		status->state = WIFI_STATE_ASSOCIATING;
	} else if (connection_state == WLAN_ASSOCIATED) {
		status->state = WIFI_STATE_ASSOCIATED;
#ifdef CONFIG_NXP_WIFI_STA_AUTO_DHCPV4
	} else if (connection_state == WLAN_CONNECTED) {
		status->state = WIFI_STATE_COMPLETED;
#else
	} else if (connection_state == WLAN_AUTHENTICATED) {
		status->state = WIFI_STATE_COMPLETED;
#endif

		if (!wlan_get_current_network(&nxp_wlan_network)) {
			strncpy(status->ssid, nxp_wlan_network.ssid, WIFI_SSID_MAX_LEN - 1);
			status->ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
			status->ssid_len = strlen(nxp_wlan_network.ssid);

			memcpy(status->bssid, nxp_wlan_network.bssid, WIFI_MAC_ADDR_LEN);

			status->rssi = nxp_wlan_network.rssi;

			status->channel = nxp_wlan_network.channel;

			status->beacon_interval = nxp_wlan_network.beacon_period;

			status->dtim_period = nxp_wlan_network.dtim_period;

			if (if_handle->state.interface == WLAN_BSS_TYPE_STA) {
				status->iface_mode = WIFI_MODE_INFRA;
			}
#ifdef CONFIG_NXP_WIFI_SOFTAP_SUPPORT
			else if (if_handle->state.interface == WLAN_BSS_TYPE_UAP) {
				status->iface_mode = WIFI_MODE_AP;
			}
#endif

#ifdef CONFIG_11AX
			if (nxp_wlan_network.dot11ax) {
				status->link_mode = WIFI_6;
			}
#endif
#ifdef CONFIG_11AC
			else if (nxp_wlan_network.dot11ac) {
				status->link_mode = WIFI_5;
			}
#endif
			else if (nxp_wlan_network.dot11n) {
				status->link_mode = WIFI_4;
			} else {
				status->link_mode = WIFI_3;
			}

			status->band = nxp_wlan_network.channel > 14 ? WIFI_FREQ_BAND_5_GHZ
								     : WIFI_FREQ_BAND_2_4_GHZ;
			status->security = nxp_wifi_security_type(nxp_wlan_network.security.type);
			status->mfp = nxp_wlan_network.security.mfpr   ? WIFI_MFP_REQUIRED
				      : nxp_wlan_network.security.mfpc ? WIFI_MFP_OPTIONAL
								       : 0;
		}
	}

	return 0;
}

#if defined(CONFIG_NET_STATISTICS_WIFI)
static int nxp_wifi_stats(const struct device *dev, struct net_stats_wifi *stats)
{
	interface_t *if_handle = (interface_t *)dev->data;

	stats->bytes.received = if_handle->stats.bytes.received;
	stats->bytes.sent = if_handle->stats.bytes.sent;
	stats->pkts.rx = if_handle->stats.pkts.rx;
	stats->pkts.tx = if_handle->stats.pkts.tx;
	stats->errors.rx = if_handle->stats.errors.rx;
	stats->errors.tx = if_handle->stats.errors.tx;
	stats->broadcast.rx = if_handle->stats.broadcast.rx;
	stats->broadcast.tx = if_handle->stats.broadcast.tx;
	stats->multicast.rx = if_handle->stats.multicast.rx;
	stats->multicast.tx = if_handle->stats.multicast.tx;
	stats->sta_mgmt.beacons_rx = if_handle->stats.sta_mgmt.beacons_rx;
	stats->sta_mgmt.beacons_miss = if_handle->stats.sta_mgmt.beacons_miss;

	return 0;
}
#endif

#ifdef CONFIG_NXP_WIFI_STA_AUTO
static void nxp_wifi_auto_connect(void)
{
	int ssid_len = strlen(CONFIG_NXP_WIFI_STA_AUTO_SSID);
	int psk_len = strlen(CONFIG_NXP_WIFI_STA_AUTO_PASSWORD);
	struct wifi_connect_req_params params = {0};
	char ssid[IEEEtypes_SSID_SIZE] = {0};
	char psk[WLAN_PSK_MAX_LENGTH] = {0};

	if (ssid_len >= IEEEtypes_SSID_SIZE) {
		LOG_ERR("AutoConnect SSID too long");
		return;
	}
	if (ssid_len == 0) {
		LOG_ERR("AutoConnect SSID NULL");
		return;
	}

	strcpy(ssid, CONFIG_NXP_WIFI_STA_AUTO_SSID);

	if (psk_len == 0) {
		params.security = WIFI_SECURITY_TYPE_NONE;
	} else if (psk_len >= WLAN_PSK_MIN_LENGTH && psk_len < WLAN_PSK_MAX_LENGTH) {
		strcpy(psk, CONFIG_NXP_WIFI_STA_AUTO_PASSWORD);
		params.security = WIFI_SECURITY_TYPE_PSK;
	} else {
		LOG_ERR("AutoConnect invalid password length %d", psk_len);
		return;
	}

	params.channel = WIFI_CHANNEL_ANY;
	params.ssid = &ssid[0];
	params.ssid_length = ssid_len;
	params.psk = &psk[0];
	params.psk_length = psk_len;

	LOG_DBG("AutoConnect SSID[%s]", ssid);
	nxp_wifi_connect(g_mlan.netif->if_dev->dev, &params);
}
#endif

static int nxp_wifi_power_save(const struct device *dev, struct wifi_ps_params *params)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	int ret = WM_SUCCESS;
	uint32_t syncBit;
	interface_t *if_handle = (interface_t *)dev->data;

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {

		switch (params->type) {
		case WIFI_PS_PARAM_STATE:
			if (params->enabled == WIFI_PS_DISABLED) {
				k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_PS_GROUP);
				ret = wlan_deepsleepps_off();
				if (ret != WM_SUCCESS) {
					status = NXP_WIFI_RET_FAIL;
				}

				if (status == NXP_WIFI_RET_SUCCESS) {
					syncBit = k_event_wait(&s_nxp_wifi_SyncEvent,
							       NXP_WIFI_SYNC_PS_GROUP, false,
							       NXP_WIFI_SYNC_TIMEOUT_MS);
					k_event_clear(&s_nxp_wifi_SyncEvent,
						      NXP_WIFI_SYNC_PS_GROUP);
					if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_PS_EXIT)) {
						status = NXP_WIFI_RET_SUCCESS;
					} else {
						status = NXP_WIFI_RET_TIMEOUT;
					}
				}

				if (status != NXP_WIFI_RET_SUCCESS) {
					LOG_DBG("Wi-Fi power save is already disabled");
					return -EAGAIN;
				}

				k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_PS_GROUP);
				ret = wlan_ieeeps_off();
				if (ret != WM_SUCCESS) {
					status = NXP_WIFI_RET_FAIL;
				}

				if (status == NXP_WIFI_RET_SUCCESS) {
					syncBit = k_event_wait(&s_nxp_wifi_SyncEvent,
							       NXP_WIFI_SYNC_PS_GROUP, false,
							       NXP_WIFI_SYNC_TIMEOUT_MS);
					k_event_clear(&s_nxp_wifi_SyncEvent,
						      NXP_WIFI_SYNC_PS_GROUP);
					if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_PS_EXIT)) {
						status = NXP_WIFI_RET_SUCCESS;
					} else {
						status = NXP_WIFI_RET_TIMEOUT;
					}
				}

				if (status != NXP_WIFI_RET_SUCCESS) {
					LOG_DBG("Wi-Fi power save is already disabled");
					return -EAGAIN;
				}
			} else if (params->enabled == WIFI_PS_ENABLED) {
				k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_PS_GROUP);
				ret = wlan_deepsleepps_on();
				if (ret != WM_SUCCESS) {
					status = NXP_WIFI_RET_FAIL;
				}

				if (status == NXP_WIFI_RET_SUCCESS) {
					syncBit = k_event_wait(&s_nxp_wifi_SyncEvent,
							       NXP_WIFI_SYNC_PS_GROUP, false,
							       NXP_WIFI_SYNC_TIMEOUT_MS);
					k_event_clear(&s_nxp_wifi_SyncEvent,
						      NXP_WIFI_SYNC_PS_GROUP);
					if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_PS_ENTER)) {
						status = NXP_WIFI_RET_SUCCESS;
					} else {
						status = NXP_WIFI_RET_TIMEOUT;
					}
				}

				if (status != NXP_WIFI_RET_SUCCESS) {
					LOG_DBG("Wi-Fi power save is already enabled");
					return -EAGAIN;
				}

				k_event_clear(&s_nxp_wifi_SyncEvent, NXP_WIFI_SYNC_PS_GROUP);
				ret = wlan_ieeeps_on((unsigned int)WAKE_ON_UNICAST |
						     (unsigned int)WAKE_ON_MAC_EVENT |
						     (unsigned int)WAKE_ON_MULTICAST |
						     (unsigned int)WAKE_ON_ARP_BROADCAST);

				if (ret != WM_SUCCESS) {
					status = NXP_WIFI_RET_FAIL;
				}

				if (status == NXP_WIFI_RET_SUCCESS) {
					syncBit = k_event_wait(&s_nxp_wifi_SyncEvent,
							       NXP_WIFI_SYNC_PS_GROUP, false,
							       NXP_WIFI_SYNC_TIMEOUT_MS);
					k_event_clear(&s_nxp_wifi_SyncEvent,
						      NXP_WIFI_SYNC_PS_GROUP);
					if (syncBit & NXP_WIFI_EVENT_BIT(WLAN_REASON_PS_ENTER)) {
						status = NXP_WIFI_RET_SUCCESS;
					} else {
						status = NXP_WIFI_RET_TIMEOUT;
					}
				}

				if (status != NXP_WIFI_RET_SUCCESS) {
					LOG_DBG("Wi-Fi power save is already enabled");
					return -EAGAIN;
				}
			}
			break;
		case WIFI_PS_PARAM_LISTEN_INTERVAL:
			wlan_configure_listen_interval((int)params->listen_interval);
			break;
		case WIFI_PS_PARAM_WAKEUP_MODE:
			if (params->wakeup_mode == WIFI_PS_WAKEUP_MODE_DTIM) {
				wlan_configure_listen_interval(0);
			}
			break;
		case WIFI_PS_PARAM_MODE:
			if (params->mode == WIFI_PS_MODE_WMM) {
				LOG_ERR("WMM power save mode not supported");
			}
			break;
		case WIFI_PS_PARAM_TIMEOUT:
			wlan_configure_delay_to_ps((int)params->timeout_ms);
			break;
		default:
			status = NXP_WIFI_RET_FAIL;
			break;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to set Wi-Fi power save");
		return -EAGAIN;
	}

	return 0;
}

int nxp_wifi_get_power_save(const struct device *dev, struct wifi_ps_config *config)
{
	nxp_wifi_ret_t status = NXP_WIFI_RET_SUCCESS;
	interface_t *if_handle = (interface_t *)dev->data;

	if (if_handle->state.interface != WLAN_BSS_TYPE_STA) {
		LOG_ERR("Wi-Fi not in station mode");
		return -EIO;
	}

	if (s_nxp_wifi_State != NXP_WIFI_STARTED) {
		status = NXP_WIFI_RET_NOT_READY;
	}

	if (status == NXP_WIFI_RET_SUCCESS) {
		if (config) {
			if (wlan_is_power_save_enabled()) {
				config->ps_params.enabled = WIFI_PS_ENABLED;
			} else {
				config->ps_params.enabled = WIFI_PS_DISABLED;
			}

			config->ps_params.listen_interval = wlan_get_listen_interval();

			config->ps_params.timeout_ms = wlan_get_delay_to_ps();
			if (config->ps_params.listen_interval) {
				config->ps_params.wakeup_mode = WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL;
			} else {
				config->ps_params.wakeup_mode = WIFI_PS_WAKEUP_MODE_DTIM;
			}

			config->ps_params.mode = WIFI_PS_MODE_LEGACY;
		} else {
			status = NXP_WIFI_RET_FAIL;
		}
	}

	if (status != NXP_WIFI_RET_SUCCESS) {
		LOG_ERR("Failed to get Wi-Fi power save config");
		return -EAGAIN;
	}

	return 0;
}

#ifdef CONFIG_RW610
extern void WL_MCI_WAKEUP0_DriverIRQHandler(void);
#endif

static void nxp_wifi_sta_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct ethernet_context *eth_ctx = net_if_l2_data(iface);
	interface_t *intf = dev->data;

	eth_ctx->eth_if_type = L2_ETH_IF_TYPE_WIFI;
	intf->netif = iface;

	net_if_flag_set(iface, NET_IF_NO_AUTO_START);

	g_mlan.state.interface = WLAN_BSS_TYPE_STA;

#ifdef CONFIG_RW610
	IRQ_CONNECT(72, 1, WL_MCI_WAKEUP0_DriverIRQHandler, 0, 0);
	irq_enable(72);
#endif
}

#ifdef CONFIG_NXP_WIFI_SOFTAP_SUPPORT

static void nxp_wifi_uap_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct ethernet_context *eth_ctx = net_if_l2_data(iface);
	interface_t *intf = dev->data;

	eth_ctx->eth_if_type = L2_ETH_IF_TYPE_WIFI;
	intf->netif = iface;

	net_if_flag_set(iface, NET_IF_NO_AUTO_START);

	g_uap.state.interface = WLAN_BSS_TYPE_UAP;
}

#endif

static int nxp_wifi_send(const struct device *dev, struct net_pkt *pkt)
{
	interface_t *if_handle = (interface_t *)dev->data;
	const int pkt_len = net_pkt_get_len(pkt);

	/* Enqueue packet for transmission */
	if (nxp_wifi_internal_tx(dev, pkt) != WM_SUCCESS) {
		goto out;
	}

#if defined(CONFIG_NET_STATISTICS_WIFI)
	if_handle->stats.bytes.sent += pkt_len;
	if_handle->stats.pkts.tx++;
#endif

	LOG_DBG("pkt sent %p len %d", pkt, pkt_len);
	return 0;

out:

	LOG_ERR("Failed to send packet");
#if defined(CONFIG_NET_STATISTICS_WIFI)
	if_handle->stats.errors.tx++;
#endif
	return -EIO;
}

static void nxp_wifi_task(void *p1, void *p2, void *p3)
{
	int ret;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Initialize the wifi subsystem */
	ret = nxp_wifi_wlan_init();
	if (ret) {
		LOG_ERR("wlan initialization failed");
		return;
	}
	ret = nxp_wifi_wlan_start();
	if (ret) {
		LOG_ERR("wlan start failed");
		return;
	}
}

#if defined(CONFIG_NXP_WIFI_SHELL)

int nxp_wifi_cmd_rsp(struct nxp_wifi_dev *nxp_wifi, char *cmd, char **rsp)
{
	int len;

	len = nxp_wifi_request(nxp_wifi, cmd, strlen(cmd), nxp_wifi->buf, sizeof(nxp_wifi->buf));
	if (len < 0) {
		return -EIO;
	}

	return 0;
}

int nxp_wifi_cmd(struct nxp_wifi_dev *nxp_wifi, char *cmd)
{
	return nxp_wifi_cmd_rsp(nxp_wifi, cmd, NULL);
}

#endif

static int nxp_wifi_dev_init(const struct device *dev)
{
	struct nxp_wifi_dev *nxp_wifi = &nxp_wifi0;

	k_mutex_init(&nxp_wifi->mutex);

	k_tid_t tid = k_thread_create(&nxp_wifi_thread, nxp_wifi_stack,
				      CONFIG_NXP_WIFI_TASK_STACK_SIZE, nxp_wifi_task, NULL, NULL,
				      NULL, CONFIG_NXP_WIFI_TASK_PRIO, K_INHERIT_PERMS, K_NO_WAIT);

	if (!tid) {
		LOG_ERR("ERROR spawning nxp_wifi thread");
		return -EAGAIN;
	}

	k_thread_name_set(tid, "nxp_wifi");

	nxp_wifi_shell_register(nxp_wifi);

	return 0;
}

static int nxp_wifi_set_config(const struct device *dev,
			       enum ethernet_config_type type,
			       const struct ethernet_config *config)
{
	interface_t *if_handle = (interface_t *)dev->data;

	switch (type) {
	case ETHERNET_CONFIG_TYPE_MAC_ADDRESS:
		memcpy(if_handle->mac_address, config->mac_address.addr, 6);

		net_if_set_link_addr(if_handle->netif, if_handle->mac_address,
				     sizeof(if_handle->mac_address),
				     NET_LINK_ETHERNET);

		if (if_handle->state.interface == WLAN_BSS_TYPE_STA) {
			if (wlan_set_sta_mac_addr(if_handle->mac_address)) {
				LOG_ERR("Failed to set Wi-Fi MAC Address");
				return -ENOEXEC;
			}
		} else if (if_handle->state.interface == WLAN_BSS_TYPE_UAP) {
			if (wlan_set_uap_mac_addr(if_handle->mac_address)) {
				LOG_ERR("Failed to set Wi-Fi MAC Address");
				return -ENOEXEC;
			}
		} else {
			LOG_ERR("Invalid Interface index");
			return -ENOEXEC;
		}
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}


static const struct wifi_mgmt_ops nxp_wifi_sta_mgmt = {
	.get_version = nxp_wifi_version,
	.scan = nxp_wifi_scan,
	.connect = nxp_wifi_connect,
	.disconnect = nxp_wifi_disconnect,
	.ap_enable = nxp_wifi_start_ap,
	.ap_bandwidth = nxp_wifi_ap_bandwidth,
	.ap_disable = nxp_wifi_stop_ap,
	.iface_status = nxp_wifi_status,
#if defined(CONFIG_NET_STATISTICS_WIFI)
	.get_stats = nxp_wifi_stats,
#endif
	.set_power_save = nxp_wifi_power_save,
	.get_power_save_config = nxp_wifi_get_power_save,
};

static const struct net_wifi_mgmt_offload nxp_wifi_sta_apis = {
	.wifi_iface.iface_api.init = nxp_wifi_sta_init,
	.wifi_iface.set_config = nxp_wifi_set_config,
	.wifi_iface.send = nxp_wifi_send,
	.wifi_mgmt_api = &nxp_wifi_sta_mgmt,
};

NET_DEVICE_INIT_INSTANCE(wifi_nxp_sta, "ml", 0, nxp_wifi_dev_init, NULL, &g_mlan, NULL,
			 CONFIG_WIFI_INIT_PRIORITY, &nxp_wifi_sta_apis, ETHERNET_L2,
			 NET_L2_GET_CTX_TYPE(ETHERNET_L2), NET_ETH_MTU);

#ifdef CONFIG_NXP_WIFI_SOFTAP_SUPPORT
static const struct wifi_mgmt_ops nxp_wifi_uap_mgmt = {
	.ap_enable = nxp_wifi_start_ap,
	.ap_disable = nxp_wifi_stop_ap,
	.iface_status = nxp_wifi_status,
#if defined(CONFIG_NET_STATISTICS_WIFI)
	.get_stats = nxp_wifi_stats,
#endif
	.set_power_save = nxp_wifi_power_save,
	.get_power_save_config = nxp_wifi_get_power_save,
};

static const struct net_wifi_mgmt_offload nxp_wifi_uap_apis = {
	.wifi_iface.iface_api.init = nxp_wifi_uap_init,
	.wifi_iface.set_config = nxp_wifi_set_config,
	.wifi_iface.send = nxp_wifi_send,
	.wifi_mgmt_api = &nxp_wifi_uap_mgmt,
};

NET_DEVICE_INIT_INSTANCE(wifi_nxp_uap, "ua", 1, NULL, NULL, &g_uap, NULL, CONFIG_WIFI_INIT_PRIORITY,
			 &nxp_wifi_uap_apis, ETHERNET_L2, NET_L2_GET_CTX_TYPE(ETHERNET_L2),
			 NET_ETH_MTU);
#endif
