//* proto.c - KNoT Application Client */

/*
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The knot client application is acting as a client that is run in Zephyr OS,
 * The client sends sensor data encapsulated using KNoT protocol.
 */

#define SYS_LOG_DOMAIN "knot"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1

#if (!defined(CONFIG_X86) && !defined(CONFIG_CPU_CORTEX_M4))
	#warning "Floating point services are currently available only for boards \
			based on the ARM Cortex-M4 or the Intel x86 architectures."
#endif

#include <zephyr.h>
#include <net/net_core.h>
#include <net/buf.h>

#include "knot.h"
#include "sm.h"
#include "proto.h"
#include "storage.h"

static struct k_thread rx_thread_data;
static K_THREAD_STACK_DEFINE(rx_stack, 1024);
static struct k_pipe *proto2net;
static struct k_pipe *net2proto;
static struct k_sem *proto2net_semaphore;
static struct k_sem *net2proto_semaphore;
K_FIFO_DEFINE(proto2net_fifo);
extern struct k_alert connection_lost;
extern struct k_alert connection_established;

struct data_item_t {
	void *fifo_reserved;
	int olen;
	u8_t opdu[128];
};

struct data_item_t tx_data;

static void proto_enqueue(u8_t *opdu, size_t olen)
{
	tx_data.olen = olen;
	memcpy(tx_data.opdu, opdu, olen);
	k_fifo_put(&proto2net_fifo, &tx_data);
}

static void proto2net_send(void)
{
	struct data_item_t *tmp;
	size_t olen;
	u8_t opdu[128];

	/* Check if clear to send */
	if (k_sem_count_get(proto2net_semaphore) == 0)
		goto done;

	/* Check if enqueued messages to send */
	if (k_fifo_is_empty(&proto2net_fifo))
		goto done;

	tmp = k_fifo_get(&proto2net_fifo, K_NO_WAIT);

	olen = tmp->olen;
	memcpy(opdu, tmp->opdu, olen);
	k_sem_take(proto2net_semaphore, K_NO_WAIT);
	k_pipe_put(proto2net, opdu, olen, &olen, olen, K_NO_WAIT);

done:
	return;
}

static void check_connection()
{
	if(k_alert_recv(&connection_lost, K_NO_WAIT) == 0)
		sm_stop();

	if(k_alert_recv(&connection_established, K_NO_WAIT) == 0)
		sm_start();
}

static void proto_thread(void)
{
	/* Considering KNOT Max MTU 128 */
	u8_t ipdu[128];
	u8_t opdu[128];
	size_t olen;
	size_t ilen;
	int ret;

	/* Initializing KNoT storage */
	storage_init();

	/* Initializing SM and abstract IO internals */
	sm_init();

	/* Calling KNoT app: setup() */
	setup();

	while (1) {
		/* Calling KNoT app: loop() */
		loop();

		/* Check if NET thread connection established */
		check_connection();

		ilen = 0;
		memset(&ipdu, 0, sizeof(ipdu));
		/* Reading data from NET thread */
		ret = k_pipe_get(net2proto, ipdu, sizeof(ipdu),
				 &ilen, 0U, K_NO_WAIT);

		/* Releases NET thread to send messages */
		if (ret == 0 && ilen)
			k_sem_give(net2proto_semaphore);

		olen = sm_run(ipdu, ilen, opdu, sizeof(opdu));

		/* Sending data to NET thread */
		if (olen != 0)
			proto_enqueue(opdu, olen);

		proto2net_send();

		/* Yelds to NET thread */
		k_yield();
	}
}

int proto_start(struct k_pipe *p2n,
		struct k_sem *p2n_sem,
		struct k_pipe *n2p,
		struct k_sem *n2p_sem)
{
	NET_DBG("PROTO: Start");

	proto2net = p2n;
	net2proto = n2p;
	proto2net_semaphore = p2n_sem;
	net2proto_semaphore= n2p_sem;

	k_thread_create(&rx_thread_data, rx_stack,
			K_THREAD_STACK_SIZEOF(rx_stack),
			(k_thread_entry_t) proto_thread,
			NULL, NULL, NULL, K_PRIO_COOP(7),
			K_FP_REGS, K_NO_WAIT);

	return 0;
}

void proto_stop(void)
{
	NET_DBG("PROTO: Stop");
}
