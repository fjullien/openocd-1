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
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <target/rx.h>

FLASH_BANK_COMMAND_HANDLER(rx_flash_bank_command)
{
/*	struct avrf_flash_bank *avrf_info;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	avrf_info = malloc(sizeof(struct avrf_flash_bank));
	bank->driver_priv = avrf_info;

	avrf_info->probed = false;*/

	return ERROR_OK;
}

static int rx_erase(struct flash_bank *bank, unsigned int first,
		unsigned int last)
{
	return ERROR_OK;
}

static int rx_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
	return ERROR_OK;
}


static int rx_probe(struct flash_bank *bank)
{
	return ERROR_OK;
}

static int rx_auto_probe(struct flash_bank *bank)
{
	return ERROR_OK;
}

static int rx_info(struct flash_bank *bank, struct command_invocation *cmd)
{
	return ERROR_OK;
}

COMMAND_HANDLER(rx_handle_mass_erase_command)
{
	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	return ERROR_OK;
}

static const struct command_registration rx_exec_command_handlers[] = {
	{
		.name = "mass_erase",
		.usage = "<bank>",
		.handler = rx_handle_mass_erase_command,
		.mode = COMMAND_EXEC,
		.help = "erase entire device",
	},
	COMMAND_REGISTRATION_DONE
};
static const struct command_registration rx_command_handlers[] = {
	{
		.name = "rx",
		.mode = COMMAND_ANY,
		.help = "rx flash command group",
		.usage = "",
		.chain = rx_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

const struct flash_driver rx_flash = {
	.name = "rx",
	.commands = rx_command_handlers,
	.flash_bank_command = rx_flash_bank_command,
	.erase = rx_erase,
	.write = rx_write,
	.read = default_flash_read,
	.probe = rx_probe,
	.auto_probe = rx_auto_probe,
	.erase_check = default_flash_blank_check,
	.info = rx_info,
	.free_driver_priv = default_flash_free_driver_priv,
};
