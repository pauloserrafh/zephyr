/* proto.h - KNoT Application Client */

/*
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

int proto_start(struct k_pipe *p2n, struct k_sem *p2n_sem,
		struct k_pipe *n2p, struct k_sem *n2p_sem);

void proto_stop(void);
