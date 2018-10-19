/* net.c - KNoT Application Client */

/*
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The knot client application is acting as a client that is run in Zephyr OS,
 * The client sends sensor data encapsulated using KNoT netcol.
 */

#define SYS_LOG_DOMAIN "knot"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1

#include <zephyr.h>
#include <net/net_core.h>
#include <net/net_pkt.h>
#include <net/buf.h>

#include "net.h"
#include "tcp6.h"

static struct k_thread rx_thread_data;
static K_THREAD_STACK_DEFINE(rx_stack, 1024);
static struct k_pipe *proto2net;
static struct k_pipe *net2proto;
static struct k_sem *proto2net_semaphore;
static struct k_sem *net2proto_semaphore;
static bool connected;
K_FIFO_DEFINE(net2proto_fifo);
K_ALERT_DEFINE(connection_lost, NULL, 1);
K_ALERT_DEFINE(connection_established, NULL, 1);

struct data_item_t {
	void *fifo_reserved;
	int olen;
	u8_t opdu[128];
};

struct data_item_t tx_data;

static void net_enqueue(u8_t *opdu, size_t olen)
{
	tx_data.olen = olen;
	memcpy(tx_data.opdu, opdu, olen);
	k_fifo_put(&net2proto_fifo, &tx_data);
}

static void net2proto_send(void)
{
	struct data_item_t *tmp;
	size_t olen;
	u8_t opdu[128];

	/* Check if clear to send */
	if (k_sem_count_get(net2proto_semaphore) == 0)
		goto done;

	/* Check if enqueued messages to send */
	if (k_fifo_is_empty(&net2proto_fifo))
		goto done;

	tmp = k_fifo_get(&net2proto_fifo, K_NO_WAIT);

	olen = tmp->olen;
	memcpy(opdu, tmp->opdu, olen);
	k_sem_take(net2proto_semaphore, K_NO_WAIT);
	k_pipe_put(net2proto, opdu, olen, &olen, olen, K_NO_WAIT);

done:
	return;
}

static void close_cb(void)
{
	if (connected)
		connected = false;
	k_alert_send(&connection_lost);
}

static bool recv_cb(struct net_buf *netbuf)
{
	u8_t opdu[128];
	struct net_buf *frag = netbuf;
	size_t olen = 0;
	size_t frag_len = 0;
	u16_t pos = 0;

	memset(opdu, 0, sizeof(opdu));

	while (frag) {
		/* Data comming from network */
		if ((olen + frag->len) > sizeof(opdu)) {
			NET_ERR("Small MTU");
			return true;
		}

		frag_len = frag->len;
		frag = net_frag_read(frag, pos, &pos, frag_len,
				     (u8_t *) (opdu + olen));
		olen += frag_len;
	}

	/* Sending recv data to PROTO thread */
	net_enqueue(opdu, olen);
	net2proto_send();

	return true;
}

static void connection_start(void)
{
	int ret;

	ret = tcp6_start(recv_cb, close_cb);
	if (ret < 0) {
		NET_DBG("NET: TCP start failure");
		tcp6_stop();
		return;
	}

	k_alert_send(&connection_established);
	connected = true;
}

static void net_thread(void)
{
	u8_t ipdu[128];
	size_t ilen;
	int ret;

	memset(ipdu, 0, sizeof(ipdu));
	connection_start();

	while (1) {
		if (!connected) {
			connection_start();
			goto done;
		}

		ilen = 0;
		/* Reading data from PROTO thread */
		ret = k_pipe_get(proto2net, ipdu, sizeof(ipdu),
				 &ilen, 0U, K_NO_WAIT);

		/* Releases PROTO thread to send messages */
		if (ret == 0 && ilen) {
			ret = tcp6_send(ipdu, ilen);
			k_sem_give(proto2net_semaphore);
		}

done:
		/* Yelds to PROTO thread */
		k_yield();
	}

	tcp6_stop();
}

int net_start(struct k_pipe *p2n,
		struct k_sem *p2n_sem,
		struct k_pipe *n2p,
		struct k_sem *n2p_sem)
{
	NET_DBG("NET: Start");

	proto2net = p2n;
	net2proto = n2p;
	proto2net_semaphore = p2n_sem;
	net2proto_semaphore= n2p_sem;
	connected = false;

	k_thread_create(&rx_thread_data, rx_stack,
			K_THREAD_STACK_SIZEOF(rx_stack),
			(k_thread_entry_t) net_thread,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	return 0;
}

void net_stop(void)
{
	NET_DBG("NET: Stop");
}
