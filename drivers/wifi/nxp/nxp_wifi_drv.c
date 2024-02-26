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
#ifdef CONFIG_PM_DEVICE
#include <zephyr/pm/device.h>
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define DT_DRV_COMPAT nxp_imu_wifi

#define IMU_IRQ_N        DT_INST_IRQ_BY_IDX(0, 0, irq)
#define IMU_IRQ_P        DT_INST_IRQ_BY_IDX(0, 0, priority)
#define IMU_WAKEUP_IRQ_N DT_INST_IRQ_BY_IDX(0, 1, irq)
#define IMU_WAKEUP_IRQ_P DT_INST_IRQ_BY_IDX(0, 1, priority)

#define MAX_JSON_NETWORK_RECORD_LENGTH 185

#define WPL_SYNC_TIMEOUT_MS K_FOREVER

#define UAP_NETWORK_NAME "uap-network"

#define EVENT_BIT(event) (1 << event)

#define WPL_SYNC_INIT_GROUP                                                                        \
	EVENT_BIT(WLAN_REASON_INITIALIZED) | EVENT_BIT(WLAN_REASON_INITIALIZATION_FAILED)

#define wifi_net_iface_num 2

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

#ifdef CONFIG_PM_DEVICE
extern int is_hs_handshake_done;
extern int wlan_host_sleep_state;
struct k_timer wake_timer;
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
int wlan_event_callback(enum wlan_event_reason reason, void *data);
static int WPL_process_results(unsigned int count);
static void LinkStatusChangeCallback(bool linkState);
static int WPL_Disconnect(const struct device *dev);
static void WPL_AutoConnect(void);
extern int low_level_output(const struct device *dev, struct net_pkt *pkt);
#ifdef CONFIG_PM_DEVICE
static void wake_timer_cb(os_timer_arg_t arg);
#endif

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
	struct wlan_network *network = NULL;

	printSeparator();
	LOG_INF("app_cb: WLAN: received event %d", reason);
	printSeparator();

	if (s_wplState >= WPL_INITIALIZED) {
		/* Do not replace the current set of events  */
		k_event_post(&s_wplSyncEvent, EVENT_BIT(reason));
	}

	switch (reason) {
	case WLAN_REASON_INITIALIZED:
		LOG_INF("app_cb: WLAN initialized");
		printSeparator();

#ifdef CONFIG_NET_INTERFACE_NAME
		ret = net_if_set_name(g_mlan.netif, "ml");
		if (ret < 0) {
			LOG_ERR("Failed to set STA network interface name");
			return 0;
		}

		ret = net_if_set_name(g_uap.netif, "ua");
		if (ret < 0) {
			LOG_ERR("Failed to set uAP network interface name");
			return 0;
		}
#endif

#ifdef CONFIG_WIFI_NXP_CLI
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
#ifdef CONFIG_PM_DEVICE
		k_timer_init(&wake_timer, wake_timer_cb, NULL);
#endif
		ret = wlan_enhanced_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize WLAN CLIs");
			return 0;
		}
		LOG_INF("ENHANCED WLAN CLIs are initialized");
		printSeparator();
#endif

#ifdef CONFIG_RF_TEST_MODE
		ret = wlan_test_mode_cli_init();
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to initialize WLAN Test Mode CLIs");
			return 0;
		}
		LOG_INF("WLAN Test Mode CLIs are initialized");
		printSeparator();
#endif

		help_command(0, NULL);
		printSeparator();
#endif

		if (IS_ENABLED(CONFIG_WIFI_NXP_STA_AUTO)) {
			WPL_AutoConnect();
		}
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

		network = (struct wlan_network *)os_mem_alloc(sizeof(struct wlan_network));
		if (!network) {
			LOG_ERR("failed to malloc sta network memory");
			return 0;
		}

		ret = wlan_get_current_network(network);
		if (ret != WM_SUCCESS) {
			LOG_ERR("Failed to get External AP network");
			goto sta_err;
		}

		LOG_INF("Connected to following BSS:");
		LOG_INF("SSID = [%s]", network->ssid);
		if (addr.ipv4.address != 0U) {
			LOG_INF("IPv4 Address: [%s]", ip);
		}
#ifdef CONFIG_IPV6
		int i;

		for (i = 0; i < CONFIG_MAX_IPV6_ADDRESSES; i++) {
			if ((addr.ipv6[i].addr_state == NET_ADDR_TENTATIVE) ||
			    (addr.ipv6[i].addr_state == NET_ADDR_PREFERRED)) {
				LOG_INF("IPv6 Address: %-13s:\t%s (%s)",
					ipv6_addr_type_to_desc((struct net_ipv6_config *)&addr.ipv6[i]),
					ipv6_addr_addr_to_desc((struct net_ipv6_config *)&addr.ipv6[i]),
					ipv6_addr_state_to_desc(addr.ipv6[i].addr_state));
			}
		}
		LOG_INF("");
#endif
		auth_fail = 0;
		s_wplStaConnected = true;
		wifi_mgmt_raise_connect_result_event(g_mlan.netif, 0);

	sta_err:
		if (network != NULL) {
			os_mem_free(network);
		}
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
		s_wplStaConnected = false;
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, 0);
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
			return 0;
		}

		LOG_INF("DHCP Server started successfully");
		printSeparator();
		s_wplUapActivated = true;
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
		s_wplUapActivated = false;
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
		syncBit = k_event_wait(&s_wplSyncEvent, WPL_SYNC_INIT_GROUP, false,
				       WPL_SYNC_TIMEOUT_MS);
		k_event_clear(&s_wplSyncEvent, WPL_SYNC_INIT_GROUP);
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

		if (params->channel == WIFI_CHANNEL_ANY) {
			uap_network.channel = 0;
		} else {
			uap_network.channel = params->channel;
		}

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
			uap_network.security.type = WLAN_SECURITY_WPA2;
			uap_network.security.key_mgmt |= WLAN_KEY_MGMT_PSK_SHA256;
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
		ret = wlan_start_network(uap_network.name);
		if (ret != WM_SUCCESS) {
			wlan_remove_network(uap_network.name);
			status = WPLRET_FAIL;
		}
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

	if ((s_wplState != WPL_STARTED) || (s_wplUapActivated != true)) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_stop_network(UAP_NETWORK_NAME);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to disable Wi-Fi AP mode");
		return -EAGAIN;
	}

	return 0;
}

static int WPL_process_results(unsigned int count)
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
	if (g_mlan.scan_cb) {
		g_mlan.scan_cb(g_mlan.netif, 0, NULL);
	}

	g_mlan.scan_cb = NULL;

	return WM_SUCCESS;
}

static int WPL_Scan(const struct device *dev, struct wifi_scan_params *params, scan_result_cb_t cb)
{
	wlan_scan_params_v2_t wlan_scan_param;
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;

	if (g_mlan.scan_cb != NULL) {
		LOG_INF("Scan callback in progress");
		return -EINPROGRESS;
	}

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		g_mlan.scan_cb = cb;
		(void)memset(&wlan_scan_param, 0, sizeof(wlan_scan_params_v2_t));
		wlan_scan_param.cb = WPL_process_results;
		if ((params != NULL) && (params->ssids[0] != NULL)) {
#ifdef CONFIG_COMBO_SCAN
			(void)memcpy(wlan_scan_param.ssid[0], params->ssids[0],
				     strlen(params->ssids[0]));
#else
			(void)memcpy(wlan_scan_param.ssid, params->ssids[0],
				     strlen(params->ssids[0]));
#endif
		}
		ret = wlan_scan_with_opt(wlan_scan_param);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to start Wi-Fi scanning");
		return -EAGAIN;
	}

	return 0;
}

static int WPL_Connect(const struct device *dev, struct wifi_connect_req_params *params)
{
	wpl_ret_t status = WPLRET_SUCCESS;
	int ret;

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	if (status == WPLRET_SUCCESS) {
		if ((params->ssid_length == 0) || (params->ssid_length > IEEEtypes_SSID_SIZE)) {
			status = WPLRET_BAD_PARAM;
		}
	}

	if (status == WPLRET_SUCCESS) {
		wlan_disconnect();

		wlan_remove_network(sta_network.name);

		wlan_initialize_sta_network(&sta_network);

		memcpy(sta_network.ssid, params->ssid, params->ssid_length);
		sta_network.ssid_specific = 1;

		if (params->channel == WIFI_CHANNEL_ANY) {
			sta_network.channel = 0;
		} else {
			sta_network.channel = params->channel;
		}

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
			sta_network.security.type = WLAN_SECURITY_WPA2;
			sta_network.security.key_mgmt |= WLAN_KEY_MGMT_PSK_SHA256;
			sta_network.security.psk_len = params->psk_length;
			strncpy(sta_network.security.psk, params->psk, params->psk_length);
		}
#endif
		else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			sta_network.security.type = WLAN_SECURITY_WPA3_SAE;
			sta_network.security.password_len = params->psk_length;
			strncpy(sta_network.security.password, params->psk, params->psk_length);
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
		ret = wlan_connect(sta_network.name);
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
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

	if (s_wplState != WPL_STARTED) {
		status = WPLRET_NOT_READY;
	}

	enum wlan_connection_state connection_state = WLAN_DISCONNECTED;
	wlan_get_connection_state(&connection_state);
	if (connection_state == WLAN_DISCONNECTED) {
		s_wplStaConnected = false;
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return WPLRET_SUCCESS;
	}

	if (status == WPLRET_SUCCESS) {
		ret = wlan_disconnect();
		if (ret != WM_SUCCESS) {
			status = WPLRET_FAIL;
		}
	}

	if (status != WPLRET_SUCCESS) {
		LOG_ERR("Failed to disconnect from AP");
		wifi_mgmt_raise_disconnect_result_event(g_mlan.netif, -1);
		return -EAGAIN;
	}

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

		if (network != NULL) {
			os_mem_free(network);
		}
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
			strncpy(status->ssid, network->ssid, WIFI_SSID_MAX_LEN - 1);
			status->ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
			status->ssid_len = strlen(status->ssid);

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

	if (network != NULL) {
		os_mem_free(network);
	}

	return 0;
}

static void WPL_AutoConnect(void)
{
	int ssid_len = strlen(CONFIG_WIFI_NXP_STA_AUTO_SSID);
	int psk_len = strlen(CONFIG_WIFI_NXP_STA_AUTO_PASSWORD);
	struct wifi_connect_req_params params = {0};
	uint8_t ssid[IEEEtypes_SSID_SIZE] = {0};
	uint8_t psk[WLAN_PSK_MAX_LENGTH] = {0};

	if (ssid_len >= IEEEtypes_SSID_SIZE) {
		LOG_ERR("AutoConnect SSID too long");
		return;
	}
	if (ssid_len == 0) {
		LOG_ERR("AutoConnect SSID NULL");
		return;
	}

	strncpy(ssid, CONFIG_WIFI_NXP_STA_AUTO_SSID, ssid_len);

	if (psk_len == 0) {
		params.security = WIFI_SECURITY_TYPE_NONE;
	} else if (psk_len >= WLAN_PSK_MIN_LENGTH && psk_len < WLAN_PSK_MAX_LENGTH) {
		strncpy(psk, CONFIG_WIFI_NXP_STA_AUTO_PASSWORD, psk_len);
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

	LOG_INF("AutoConnect SSID[%s]", ssid);
	WPL_Connect(g_mlan.netif->if_dev->dev, &params);
}

#ifdef RW610
extern void WL_MCI_WAKEUP0_DriverIRQHandler(void);
extern void WL_MCI_WAKEUP_DONE0_DriverIRQHandler(void);
#endif

static int wifi_net_init(const struct device *dev)
{
	return 0;
}

static void wifi_net_iface_init(struct net_if *iface)
{
	static int init_cnt;
	const struct device *dev = net_if_get_device(iface);
	interface_t *intf = dev->data;
	struct ethernet_context *eth_ctx = net_if_l2_data(iface);
	int ret;

	intf->netif = iface;
	net_if_flag_set(iface, NET_IF_NO_AUTO_START);
	eth_ctx->eth_if_type = L2_ETH_IF_TYPE_WIFI;
	init_cnt++;

	/* Not do the wlan init until the last wifi netif configured */
	if (init_cnt == wifi_net_iface_num) {
		g_mlan.state.interface = WLAN_BSS_TYPE_STA;
		g_uap.state.interface = WLAN_BSS_TYPE_UAP;

#ifdef RW610
		IRQ_CONNECT(IMU_IRQ_N, IMU_IRQ_P, WL_MCI_WAKEUP0_DriverIRQHandler, 0, 0);
		irq_enable(IMU_IRQ_N);
		IRQ_CONNECT(IMU_WAKEUP_IRQ_N, IMU_WAKEUP_IRQ_P, WL_MCI_WAKEUP_DONE0_DriverIRQHandler, 0, 0);
		irq_enable(IMU_WAKEUP_IRQ_N);
#if (DT_INST_PROP(0, wakeup_source))
		EnableDeepSleepIRQ(IMU_IRQ_N);
#endif
#endif
		/* Initialize the wifi subsystem */
		ret = WPL_Init();
		if (ret) {
			LOG_ERR("wlan initialization failed");
			return;
		}
		ret = WPL_Start(LinkStatusChangeCallback);
		if (ret) {
			LOG_ERR("wlan start failed");
			return;
		}
	}
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

#ifdef CONFIG_PM_DEVICE
static void wake_timer_cb(os_timer_arg_t arg)
{
	if(wakelock_isheld())
		wakelock_put();
}

void device_pm_dump_wakeup_source()
{
	if(POWER_GetWakeupStatus(IMU_IRQ_N))
	{
		LOG_INF("Wakeup by WLAN");
		POWER_ClearWakeupStatus(IMU_IRQ_N);
	}
	else if(POWER_GetWakeupStatus(41))
	{
		LOG_INF("Wakeup by OSTIMER");
		POWER_ClearWakeupStatus(41);
	}
}

static int device_wlan_pm_action(const struct device *dev,
				 enum pm_device_action pm_action)
{
	int ret = 0;

	switch(pm_action)
	{
		case PM_DEVICE_ACTION_SUSPEND:
			if(!wlan_host_sleep_state || !wlan_is_started() || wakelock_isheld()
#ifdef CONFIG_WMM_UAPSD
			   || wlan_is_wmm_uapsd_enabled()
#endif
			)
				return -EBUSY;
			/* Trigger host sleep handshake here. Before handshake is done, host is not allowed to enter low power mode */
			if (!is_hs_handshake_done)
			{
				is_hs_handshake_done = WLAN_HOSTSLEEP_IN_PROCESS;
				ret = powerManager_send_event(HOST_SLEEP_HANDSHAKE, NULL);
				if (ret != 0)
					return -EFAULT;
				return -EBUSY;
			}
			if (is_hs_handshake_done == WLAN_HOSTSLEEP_IN_PROCESS)
				return -EBUSY;
			if (is_hs_handshake_done == WLAN_HOSTSLEEP_FAIL)
			{
				is_hs_handshake_done = 0;
				return -EFAULT;
			}
			break;
		case PM_DEVICE_ACTION_RESUME:
			/* Cancel host sleep in firmware and dump wakekup source.
			 * If sleep state is periodic, start timer to keep host in full power state for 5s.
			 * User can use this time to issue other commands.
			 */
			if (is_hs_handshake_done == WLAN_HOSTSLEEP_SUCCESS)
			{
				ret = powerManager_send_event(HOST_SLEEP_EXIT, NULL);
				if (ret != 0)
					return -EFAULT;
				device_pm_dump_wakeup_source();
				/* reset hs hanshake flag after waking up */
				is_hs_handshake_done = 0;
				if(wlan_host_sleep_state == HOST_SLEEP_ONESHOT)
					wlan_host_sleep_state = HOST_SLEEP_DISABLE;
				else if(wlan_host_sleep_state == HOST_SLEEP_PERIODIC)
				{
					wakelock_get();
					k_timer_start(&wake_timer, K_TICKS(50000), K_NO_WAIT);
				}
			}
			break;
		default:
			break;
	}
	return ret;
}

int wlan_pm_init(const struct device *dev)
{
	return 0;
}

PM_DEVICE_DT_INST_DEFINE(0, device_wlan_pm_action);
#endif

NET_DEVICE_INIT_INSTANCE(wifi_nxp_sta, "ml", 0, wifi_net_init, PM_DEVICE_DT_INST_GET(0),
			 &g_mlan,
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
