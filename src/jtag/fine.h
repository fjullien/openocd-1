/***************************************************************************
 *   Copyright (C) 2021 by Franck Jullien                                  *
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

#ifndef OPENOCD_JTAG_FINE_H
#define OPENOCD_JTAG_FINE_H

struct fine_driver {
	/**
	 * Send SRST (system reset) command to target.
	 *
	 * @return ERROR_OK on success, else a fault code.
	 */
	int (*srst)(void);

	/**
	 * Initialize the debug link so it can perform FINE operations.
	 *
	 * Performs transition in Boot Mode.
	 *
	 * @return ERROR_OK on success, else a negative fault code.
	 */
	int (*init)(void);

	/**
	 * Send a command to the target and receive a status packet.
	 * 
	 * @param out Buffer with data to be sent
	 * @param in Where to store value to read from target
	 * @param out_cnt Number of data to send
	 * @param in_cnt Number of expected read data
	 * @param timeout Time to wait for in_cnt bytes to be received
	 * @return ERROR_OK on success, else received error code from the target.
	 */
	int (*xfer)(uint8_t *out, uint8_t *in, uint8_t out_cnt, uint8_t in_cnt,
		    int timeout);
};

int fine_init_reset(struct command_context *cmd_ctx);

static inline uint32_t buf_get_u32_be(const uint8_t *_buffer, int offset)
{
	const uint8_t *buffer = _buffer;
	return (buffer[offset] << 24) + (buffer[offset + 1] << 16) + (buffer[offset + 2] << 8) + buffer[offset + 3];
}

static inline void buf_set_u32_be(uint8_t *_buffer, int offset, uint32_t val)
{
	uint8_t *buffer = _buffer;
	buffer[offset]     = (val >> 24) & 0xFF;
	buffer[offset + 1] = (val >> 16) & 0xFF;
	buffer[offset + 2] = (val >> 8) & 0xFF;
	buffer[offset + 3] = val & 0xFF;
}

#endif /* OPENOCD_JTAG_FINE_H */
