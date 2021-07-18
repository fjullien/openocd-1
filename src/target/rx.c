/***************************************************************************
 *   Copyright (C) 2021 by Franck Jullien                                  *
 *   franck.jullien@gmail.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rx.h"
#include "target.h"
#include "target_type.h"

#include <jtag/jtag.h>
#include <transport/transport.h>

static int rx_poll(struct target *target)
{
	if ((target->state == TARGET_RUNNING) || (target->state == TARGET_DEBUG_RUNNING))
		target->state = TARGET_HALTED;

	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int rx_target_create(struct target *target, Jim_Interp *interp)
{
	struct rx *rx = calloc(1, sizeof(struct rx));

	rx->tap = target->tap;
	target->arch_info = rx;

	return ERROR_OK;
}

static int rx_init_target(struct command_context *cmd_ctx, struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

struct target_type rx_target = {
	.name = "rx",

	.target_create = rx_target_create,
	.init_target = rx_init_target,

	.poll = rx_poll,
};
