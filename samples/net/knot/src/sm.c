/* sm.c - KNoT Application Client */

/*
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* KNoT State Machine */

#define SYS_LOG_DOMAIN "knot"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1

#include <zephyr.h>
#include <net/net_core.h>

#include "sm.h"

// Variables and defines used for test only.
#define TOTALITEMS 4
#define TIMEOUT 3000

static int is_registered = 0;
static int sent_schemas = 0;
static int count;
////////////////////////////////////////////

static enum sm_state state = DISCONNECTED;

int sm_start(void)
{
	NET_DBG("SM: State Machine start");
	state = CONNECTED;

	return 0;
}

void sm_stop(void)
{
	NET_DBG("SM: Stop");
	state = DISCONNECTED;
}

int sm_run(const unsigned char *ipdu, size_t ilen,
	   unsigned char *opdu, size_t olen)
{
	memset(opdu, 0, sizeof(olen));

	switch (state) {
	/*
	 * If the thing has credentials stored, send auth request.
	 * Otherwise send register request.
	 */
	case CONNECTED:
		//TODO: Read credentials from storage
		strcpy(opdu, "CONN");

		state = AUTHENTICATE;
		if (!is_registered)
			//TODO: Send register request
			state = REGISTER;
		break;
	case AUTHENTICATE:
		//TODO: Send credentials to authenticate on cloud
		strcpy(opdu, "AUTH");

		state = ONLINE;
		if (!sent_schemas) {
			state = SCHEMA;
			sent_schemas = 1;
		}
		break;
	case REGISTER:
		//TODO: Save credentials on storage
		strcpy(opdu, "REG");

		state = SCHEMA;
		is_registered = 1;
		count = 0;
		break;
	case SCHEMA:
		//TODO: Send schemas
		strcpy(opdu, "SCHM");

		state = SCHEMA_RESP;
		break;
	/*
	 * Receives the ack from the GW and returns to STATE_SCHEMA to send the
	 * next schema. If it was the ack for the last schema, goes to
	 * STATE_ONLINE.
	 */
	case SCHEMA_RESP:
		//TODO: Wait for the schema ack
		strcpy(opdu, "SCHRES");

		state = SCHEMA;
		if (count >= TOTALITEMS)
			state = ONLINE;

		count++;
		break;
	/*
	 * As soon as the thing starts, sends the status of each item.
	 */
	case ONLINE:
		//TODO: Send the current status of each item
		strcpy(opdu, "ONLN");

		state = ONLINE;
		if (count >= TOTALITEMS)
			state = RUN;

		count++;
		break;
	case RUN:
		//TODO: Check for incoming messages and/or changes on sensors
		strcpy(opdu, "RUN");

		state = DISCONNECTED;
		break;
	case DISCONNECTED:
		strcpy(opdu, "DISC");

		state = CONNECTED;
		break;
	}

	return strlen(opdu);
}
