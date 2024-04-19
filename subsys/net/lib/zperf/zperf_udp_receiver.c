/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(net_zperf, CONFIG_NET_ZPERF_LOG_LEVEL);

#include <zephyr/linker/sections.h>
#include <zephyr/toolchain.h>

#include <zephyr/kernel.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/zperf.h>

#include "zperf_internal.h"
#include "zperf_session.h"

/* To get net_sprint_ipv{4|6}_addr() */
#define NET_LOG_ENABLED 1
#include "net_private.h"

/* To support multicast */
#include "ipv6.h"
#include "zephyr/net/igmp.h"

static struct sockaddr_in6 *in6_addr_my;
static struct sockaddr_in *in4_addr_my;

#if defined(CONFIG_NET_TC_THREAD_PRIO_CUSTOM)
#define UDP_RECEIVER_THREAD_PRIORITY CONFIG_ZPERF_UDP_RECEIVER_THREAD_PRIO
#elif defined(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define UDP_RECEIVER_THREAD_PRIORITY K_PRIO_COOP(8)
#else
#define UDP_RECEIVER_THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif

#define UDP_RECEIVER_STACK_SIZE 2048

#define SOCK_ID_IPV4 0
#define SOCK_ID_IPV6 1
#define SOCK_ID_MAX 2

#define UDP_RECEIVER_BUF_SIZE 1500
#define POLL_TIMEOUT_MS 100

static K_THREAD_STACK_DEFINE(udp_receiver_stack_area, UDP_RECEIVER_STACK_SIZE);
static struct k_thread udp_receiver_thread_data;

static zperf_callback udp_session_cb;
static void *udp_user_data;
static bool udp_server_running;
static bool udp_server_stop;
static uint16_t udp_server_port;
static struct sockaddr udp_server_addr;
static char udp_server_iface_name[Z_DEVICE_MAX_NAME_LEN];
static K_SEM_DEFINE(udp_server_run, 0, 1);

static inline void build_reply(struct zperf_udp_datagram *hdr,
			       struct zperf_server_hdr *stat,
			       uint8_t *buf)
{
	int pos = 0;
	struct zperf_server_hdr *stat_hdr;

	memcpy(&buf[pos], hdr, sizeof(struct zperf_udp_datagram));
	pos += sizeof(struct zperf_udp_datagram);

	stat_hdr = (struct zperf_server_hdr *)&buf[pos];

	stat_hdr->flags = htonl(stat->flags);
	stat_hdr->total_len1 = htonl(stat->total_len1);
	stat_hdr->total_len2 = htonl(stat->total_len2);
	stat_hdr->stop_sec = htonl(stat->stop_sec);
	stat_hdr->stop_usec = htonl(stat->stop_usec);
	stat_hdr->error_cnt = htonl(stat->error_cnt);
	stat_hdr->outorder_cnt = htonl(stat->outorder_cnt);
	stat_hdr->datagrams = htonl(stat->datagrams);
	stat_hdr->jitter1 = htonl(stat->jitter1);
	stat_hdr->jitter2 = htonl(stat->jitter2);
}

/* Send statistics to the remote client */
#define BUF_SIZE sizeof(struct zperf_udp_datagram) +	\
	sizeof(struct zperf_server_hdr)

static int zperf_receiver_send_stat(int sock, const struct sockaddr *addr,
				    struct zperf_udp_datagram *hdr,
				    struct zperf_server_hdr *stat)
{
	uint8_t reply[BUF_SIZE];
	int ret;

	build_reply(hdr, stat, reply);

	ret = zsock_sendto(sock, reply, sizeof(reply), 0, addr,
			   addr->sa_family == AF_INET6 ?
			   sizeof(struct sockaddr_in6) :
			   sizeof(struct sockaddr_in));
	if (ret < 0) {
		NET_ERR("Cannot send data to peer (%d)", errno);
	}

	return ret;
}

static void udp_received(int sock, const struct sockaddr *addr, uint8_t *data,
			 size_t datalen)
{
	struct zperf_udp_datagram *hdr;
	struct session *session;
	int32_t transit_time;
	int64_t time;
	int32_t id;

	if (datalen < sizeof(struct zperf_udp_datagram)) {
		NET_WARN("Short iperf packet!");
		return;
	}

	hdr = (struct zperf_udp_datagram *)data;
	time = k_uptime_ticks();

	session = get_session(addr, SESSION_UDP);
	if (!session) {
		NET_ERR("Cannot get a session!");
		return;
	}

	id = ntohl(hdr->id);

	switch (session->state) {
	case STATE_COMPLETED:
	case STATE_NULL:
		if (id < 0) {
			/* Session is already completed: Resend the stat packet
			 * and continue
			 */
			if (zperf_receiver_send_stat(sock, addr, hdr,
						     &session->stat) < 0) {
				NET_ERR("Failed to send the packet");
			}
		} else {
			zperf_reset_session_stats(session);
			session->state = STATE_ONGOING;
			session->start_time = time;

			/* Start a new session! */
			if (udp_session_cb != NULL) {
				udp_session_cb(ZPERF_SESSION_STARTED, NULL,
					       udp_user_data);
			}
		}
		break;
	case STATE_ONGOING:
		if (id < 0) { /* Negative id means session end. */
			struct zperf_results results = { 0 };
			uint32_t duration;

			duration = k_ticks_to_us_ceil32(time -
							session->start_time);

			/* Update state machine */
			session->state = STATE_COMPLETED;

			/* Fill statistics */
			session->stat.flags = 0x80000000;
			session->stat.total_len1 = session->length >> 32;
			session->stat.total_len2 =
				session->length % 0xFFFFFFFF;
			session->stat.stop_sec = duration / USEC_PER_SEC;
			session->stat.stop_usec = duration % USEC_PER_SEC;
			session->stat.error_cnt = session->error;
			session->stat.outorder_cnt = session->outorder;
			session->stat.datagrams = session->counter;
			session->stat.jitter1 = 0;
			session->stat.jitter2 = session->jitter;

			if (zperf_receiver_send_stat(sock, addr, hdr,
						     &session->stat) < 0) {
				NET_ERR("Failed to send the packet");
			}

			results.nb_packets_rcvd = session->counter;
			results.nb_packets_lost = session->error;
			results.nb_packets_outorder = session->outorder;
			results.total_len = session->length;
			results.time_in_us = duration;
			results.jitter_in_us = session->jitter;
			results.packet_size = session->length / session->counter;

			if (udp_session_cb != NULL) {
				udp_session_cb(ZPERF_SESSION_FINISHED, &results,
					       udp_user_data);
			}
		} else {
			/* Update counter */
			session->counter++;
			session->length += datalen;

			/* Compute jitter */
			transit_time = time_delta(
				k_ticks_to_us_ceil32(time),
				ntohl(hdr->tv_sec) * USEC_PER_SEC +
				ntohl(hdr->tv_usec));
			if (session->last_transit_time != 0) {
				int32_t delta_transit = transit_time -
					session->last_transit_time;

				delta_transit =
					(delta_transit < 0) ?
					-delta_transit : delta_transit;

				session->jitter +=
					(delta_transit - session->jitter) / 16;
			}

			session->last_transit_time = transit_time;

			/* Check header id */
			if (id != session->next_id) {
				if (id < session->next_id) {
					session->outorder++;
				} else {
					session->error += id - session->next_id;
					session->next_id = id + 1;
				}
			} else {
				session->next_id++;
			}
		}
		break;
	default:
		break;
	}
}

static void zperf_udp_join_mcast_ipv4(char *if_name, struct in_addr *addr)
{
	struct net_if *iface = NULL;

	if (if_name[0]) {
		iface = net_if_get_by_index(net_if_get_by_name(if_name));
		if (iface == NULL)
			iface = net_if_get_default();
	} else {
		iface = net_if_get_default();
	}

	if (iface != NULL) {
		net_ipv4_igmp_join(iface, addr);
	}
}

static void zperf_udp_join_mcast_ipv6(char *if_name, struct in6_addr *addr)
{
	struct net_if *iface = NULL;

	if (if_name[0]) {
		iface = net_if_get_by_index(net_if_get_by_name(if_name));
		if (iface == NULL)
			iface = net_if_get_default();
	} else {
		iface = net_if_get_default();
	}

	if (iface != NULL) {
		net_ipv6_mld_join(iface, addr);
	}
}

static void zperf_udp_leave_mcast(int sock)
{
	struct net_if *iface = NULL;
	struct sockaddr addr = {0};
	int addr_len = NET_IPV6_ADDR_SIZE;

	zsock_getsockname(sock, &addr, &addr_len);

	if (IS_ENABLED(CONFIG_NET_IPV4) && addr.sa_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;

		if (net_ipv4_is_addr_mcast(&addr4->sin_addr))
			net_ipv4_igmp_leave(iface, &addr4->sin_addr);
	}

	if (IS_ENABLED(CONFIG_NET_IPV6) && addr.sa_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;

		if (net_ipv6_is_addr_mcast(&addr6->sin6_addr))
			net_ipv6_mld_leave(iface, &addr6->sin6_addr);
	}
}

static void udp_server_session(void)
{
	static uint8_t buf[UDP_RECEIVER_BUF_SIZE];
	struct zsock_pollfd fds[SOCK_ID_MAX] = { 0 };
	int ret;
	int family;

	for (int i = 0; i < ARRAY_SIZE(fds); i++) {
		fds[i].fd = -1;
	}

	family = udp_server_addr.sa_family;

	if (IS_ENABLED(CONFIG_NET_IPV4) &&
		(family == AF_INET || family == AF_UNSPEC)) {
		const struct in_addr *in4_addr = NULL;

		in4_addr_my = zperf_get_sin();

		fds[SOCK_ID_IPV4].fd = zsock_socket(AF_INET, SOCK_DGRAM,
						    IPPROTO_UDP);
		if (fds[SOCK_ID_IPV4].fd < 0) {
			NET_ERR("Cannot create IPv4 network socket.");
			goto error;
		}

		in4_addr = &net_sin(&udp_server_addr)->sin_addr;

		if (!net_ipv4_is_addr_unspecified(in4_addr)) {
			memcpy(&in4_addr_my->sin_addr, in4_addr,
				sizeof(struct in_addr));
		} else if (strlen(MY_IP4ADDR ? MY_IP4ADDR : "")) {
			/* Use setting IP */
			ret = zperf_get_ipv4_addr(MY_IP4ADDR,
						  &in4_addr_my->sin_addr);
			if (ret < 0) {
				NET_WARN("Unable to set IPv4");
				goto use_any_ipv4;
			}
		} else if (udp_server_iface_name[0]) {
			in4_addr = zperf_get_if_in4_addr(&udp_server_iface_name[0],
				strlen(udp_server_iface_name));
			if (!in4_addr) {
				NET_ERR("Unable to get IPv4 by iface name %s",
					udp_server_iface_name);
				goto error;
			}
			memcpy(&in4_addr_my->sin_addr, in4_addr,
				sizeof(struct in_addr));
		} else {
use_any_ipv4:
			in4_addr_my->sin_addr.s_addr = INADDR_ANY;
		}

		if (net_ipv4_is_addr_mcast(&in4_addr_my->sin_addr))
			zperf_udp_join_mcast_ipv4(udp_server_iface_name,
				&in4_addr_my->sin_addr);

		NET_INFO("Binding to %s",
			 net_sprint_ipv4_addr(&in4_addr_my->sin_addr));

		in4_addr_my->sin_port = htons(udp_server_port);

		ret = zsock_bind(fds[SOCK_ID_IPV4].fd,
				 (struct sockaddr *)in4_addr_my,
				 sizeof(struct sockaddr_in));
		if (ret < 0) {
			NET_ERR("Cannot bind IPv4 UDP port %d (%d)",
				ntohs(in4_addr_my->sin_port),
				errno);
			goto error;
		}

		fds[SOCK_ID_IPV4].events = ZSOCK_POLLIN;
	}

	if (IS_ENABLED(CONFIG_NET_IPV6) &&
		(family == AF_INET6 || family == AF_UNSPEC)) {
		const struct in6_addr *in6_addr = NULL;

		in6_addr_my = zperf_get_sin6();

		fds[SOCK_ID_IPV6].fd = zsock_socket(AF_INET6, SOCK_DGRAM,
						    IPPROTO_UDP);
		if (fds[SOCK_ID_IPV6].fd < 0) {
			NET_ERR("Cannot create IPv4 network socket.");
			goto error;
		}

		in6_addr = &net_sin6(&udp_server_addr)->sin6_addr;

		if (!net_ipv6_is_addr_unspecified(in6_addr)) {
			memcpy(&in6_addr_my->sin6_addr, in6_addr,
				sizeof(struct in6_addr));
		} else if (strlen(MY_IP6ADDR ? MY_IP6ADDR : "")) {
			/* Use setting IP */
			ret = zperf_get_ipv6_addr(MY_IP6ADDR,
						  MY_PREFIX_LEN_STR,
						  &in6_addr_my->sin6_addr);
			if (ret < 0) {
				NET_WARN("Unable to set IPv6");
				goto use_any_ipv6;
			}
		} else {
use_any_ipv6:
			memcpy(&in6_addr_my->sin6_addr,
			       net_ipv6_unspecified_address(),
			       sizeof(struct in6_addr));
		}

		if (net_ipv6_is_addr_mcast(&in6_addr_my->sin6_addr))
			zperf_udp_join_mcast_ipv6(udp_server_iface_name,
				&in6_addr_my->sin6_addr);

		NET_INFO("Binding to %s",
			 net_sprint_ipv6_addr(&in6_addr_my->sin6_addr));

		in6_addr_my->sin6_port = htons(udp_server_port);

		ret = zsock_bind(fds[SOCK_ID_IPV6].fd,
				 (struct sockaddr *)in6_addr_my,
				 sizeof(struct sockaddr_in6));
		if (ret < 0) {
			NET_ERR("Cannot bind IPv6 UDP port %d (%d)",
				ntohs(in6_addr_my->sin6_port),
				ret);
			goto error;
		}

		fds[SOCK_ID_IPV6].events = ZSOCK_POLLIN;
	}

	NET_INFO("Listening on port %d", udp_server_port);

	while (true) {
		ret = zsock_poll(fds, ARRAY_SIZE(fds), POLL_TIMEOUT_MS);
		if (ret < 0) {
			NET_ERR("UDP receiver poll error (%d)", errno);
			goto error;
		}

		if (udp_server_stop) {
			goto cleanup;
		}

		if (ret == 0) {
			continue;
		}

		for (int i = 0; i < ARRAY_SIZE(fds); i++) {
			struct sockaddr addr;
			socklen_t addrlen = sizeof(addr);

			if ((fds[i].revents & ZSOCK_POLLERR) ||
			    (fds[i].revents & ZSOCK_POLLNVAL)) {
				NET_ERR("UDP receiver IPv%d socket error",
					(i == SOCK_ID_IPV4) ? 4 : 6);
				goto error;
			}

			if (!(fds[i].revents & ZSOCK_POLLIN)) {
				continue;
			}

			ret = zsock_recvfrom(fds[i].fd, buf, sizeof(buf), 0,
					     &addr, &addrlen);
			if (ret < 0) {
				NET_ERR("recv failed on IPv%d socket (%d)",
					(i == SOCK_ID_IPV4) ? 4 : 6, errno);
				goto error;
			}

			udp_received(fds[i].fd, &addr, buf, ret);
		}
	}

error:
	if (udp_session_cb != NULL) {
		udp_session_cb(ZPERF_SESSION_ERROR, NULL, udp_user_data);
	}

cleanup:
	for (int i = 0; i < ARRAY_SIZE(fds); i++) {
		if (fds[i].fd >= 0) {
			zperf_udp_leave_mcast(fds[i].fd);
			zsock_close(fds[i].fd);
		}
	}
}

static void udp_receiver_thread(void *ptr1, void *ptr2, void *ptr3)
{
	ARG_UNUSED(ptr1);
	ARG_UNUSED(ptr2);
	ARG_UNUSED(ptr3);

	while (true) {
		k_sem_take(&udp_server_run, K_FOREVER);

		udp_server_session();

		udp_server_running = false;
	}
}

void zperf_udp_receiver_init(void)
{
	k_thread_create(&udp_receiver_thread_data,
			udp_receiver_stack_area,
			K_THREAD_STACK_SIZEOF(udp_receiver_stack_area),
			udp_receiver_thread,
			NULL, NULL, NULL,
			UDP_RECEIVER_THREAD_PRIORITY,
			IS_ENABLED(CONFIG_USERSPACE) ? K_USER |
						       K_INHERIT_PERMS : 0,
			K_NO_WAIT);
}

int zperf_udp_download(const struct zperf_download_params *param,
		       zperf_callback callback, void *user_data)
{
	if (param == NULL || callback == NULL) {
		return -EINVAL;
	}

	if (udp_server_running) {
		return -EALREADY;
	}

	udp_session_cb = callback;
	udp_user_data  = user_data;
	udp_server_port = param->port;
	udp_server_running = true;
	udp_server_stop = false;
	memcpy(&udp_server_addr, &param->addr, sizeof(struct sockaddr));

	if (param->if_name[0]) {
		memset(udp_server_iface_name, 0x0, sizeof(udp_server_iface_name));
		strncpy(udp_server_iface_name, param->if_name,
			sizeof(udp_server_iface_name));
	} else {
		udp_server_iface_name[0] = 0;
	}

	k_sem_give(&udp_server_run);

	return 0;
}

int zperf_udp_download_stop(void)
{
	if (!udp_server_running) {
		return -EALREADY;
	}

	udp_server_stop = true;
	udp_session_cb = NULL;

	return 0;
}
