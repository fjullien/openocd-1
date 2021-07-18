/*
*   OpenOCD STM8 target driver
*   Copyright (C) 2017  Ake Rehnman
*   ake.rehnman(at)gmail.com
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPENOCD_TARGET_RX_H
#define OPENOCD_TARGET_RX_H

#include "target.h"

struct mem_area {
	int koa;
	uint32_t sad;
	uint32_t ead;
	uint32_t eau;
	uint32_t wau;
};

struct rx {
	struct jtag_tap *tap;

	/* Data from Device Type Acquisition Command */
	uint8_t type[8];
	int max_input_clk_freq;
	int min_input_clk_freq;
	int max_sys_clk_freq;
	int min_sys_clk_freq;

	/* Data from Frequency Setting Command */
	int sys_clk_freq;
	int periph_clk_freq;

	/* Authentification ID */
	uint8_t id_code[16];

	/* Authentification mode */
	int serial_allowed;

	/* Area informations */
	int area_count;
	struct mem_area area[8];
};

static inline struct rx* target_to_rx(struct target *target)
{
	return target->arch_info;
}

#endif /* OPENOCD_TARGET_RX_H */
