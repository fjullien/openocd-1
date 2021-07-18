/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2021 by Franck Jullien <franck.jullien@gmail.com
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "interface.h"
#include "fine.h"

#include <target/rx.h>
#include <helper/command.h>
#include <transport/transport.h>

#include <libjaylink/libjaylink.h>

#define PKT_CMD				0
#define PKT_STATUS			0x80

#define FINE_TIMEOUT			0x64
#define FINE_RETRY_ID_COUNT		10

#define FINE_START_SEQ			0x9D4375C0

#define FINE_GET_CHIP_ID		0xC2
#define FINE_ASK_TARGET_ACK		0xC6
#define FINE_ASK_TARGET_DATA		0xC4

#define FINE_CMD_SYNC			0x00
#define FINE_CMD_GET_AUTH_MODE		0x2C
#define FINE_CMD_CHECK_ID_CODE		0x30
#define FINE_CMD_SET_FREQUENCY		0x32
#define FINE_CMD_SET_BITRATE		0x34
#define FINE_CMD_SET_ENDIANNSESS	0x36
#define FINE_CMD_GET_DEVICE_TYPE	0x38
#define FINE_CMD_GET_AREA_COUNT		0x53
#define FINE_CMD_GET_AREA_INFO		0x54

#define FINE_CMD_SOH			0x01
#define FINE_CMD_ETX			0x03

#define FINE_CMD_ERR_NOT_SUPPORTED	0xC0
#define FINE_CMD_ERR_PACKET		0xC1
#define FINE_CMD_ERR_CHECKSUM		0xC2
#define FINE_CMD_ERR_FLOW		0xC3
#define FINE_CMD_ERR_ADDRESS		0xD0
#define FINE_CMD_ERR_INPUT_FREQU	0xD1
#define FINE_CMD_ERR_SYS_CLK		0xD2
#define FINE_CMD_ERR_AREA		0xD5
#define FINE_CMD_ERR_BIT_RATE		0xD4
#define FINE_CMD_ERR_ENDIAN		0xD7
#define FINE_CMD_ERR_PROTECTION		0xDA
#define FINE_CMD_ERR_WRONG_ID		0xDB
#define FINE_CMD_ERR_SERIAL_CNX		0xDC
#define FINE_CMD_ERR_NON_BLANK		0xE0
#define FINE_CMD_ERR_ERASE		0xE1
#define FINE_CMD_ERR_PROGRAM		0xE2
#define FINE_CMD_ERR_FLASH_SEQ		0xE7

extern struct adapter_driver *adapter_driver;

COMMAND_HANDLER(handle_fine_id_command)
{
	struct target *t = get_current_target(CMD_CTX);
	struct rx *rx_device = target_to_rx(t);
	int ret;

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	ret = unhexify(rx_device->id_code, CMD_ARGV[0], 16);
	if (ret != 16) {
		LOG_ERROR("ID format is wrong");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	LOG_INFO("Authentification ID set to %s", CMD_ARGV[0]);

	return ERROR_OK;
}

COMMAND_HANDLER(handle_fine_newtap_command)
{
	struct jtag_tap *tap;

	/*
	 * only need "basename" and "tap_type", but for backward compatibility
	 * ignore extra parameters
	 */
	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	tap = calloc(1, sizeof(*tap));
	if (!tap) {
		LOG_ERROR("Out of memory");
		return ERROR_FAIL;
	}

	tap->chip = strdup(CMD_ARGV[0]);
	tap->tapname = strdup(CMD_ARGV[1]);
	tap->dotted_name = alloc_printf("%s.%s", CMD_ARGV[0], CMD_ARGV[1]);
	if (!tap->chip || !tap->tapname || !tap->dotted_name) {
		LOG_ERROR("Out of memory");
		free(tap->dotted_name);
		free(tap->tapname);
		free(tap->chip);
		free(tap);
		return ERROR_FAIL;
	}

	LOG_DEBUG("Creating new fine \"tap\", Chip: %s, Tap: %s, Dotted: %s",
			  tap->chip, tap->tapname, tap->dotted_name);

	/* default is enabled-after-reset */
	tap->enabled = true;

	jtag_tap_init(tap);
	return ERROR_OK;
}

static const struct command_registration fine_transport_subcommand_handlers[] = {
	{
		.name = "newtap",
		.handler = handle_fine_newtap_command,
		.mode = COMMAND_CONFIG,
		.help = "Create a new TAP instance named basename.tap_type, "
				"and appends it to the scan chain.",
		.usage = "basename tap_type",
	},
	{
		.name = "id",
		.handler = handle_fine_id_command,
		.mode = COMMAND_CONFIG,
		.help = "Set authentification ID for this device.",
		.usage = "value",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration fine_transport_command_handlers[] = {
	{
		.name = "fine",
		.mode = COMMAND_ANY,
		.help = "perform fine adapter actions",
		.usage = "",
		.chain = fine_transport_subcommand_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

static int fine_transport_select(struct command_context *cmd_ctx)
{
	const struct fine_driver *fine = adapter_driver->fine_ops;
	int retval;

	retval = register_commands(cmd_ctx, NULL, fine_transport_command_handlers);
	if (retval != ERROR_OK)
		return retval;

	 /* be sure driver is in SWD mode; start
	  * with hardware default TRN (1), it can be changed later
	  */
	if (!fine /*|| !swd->read_reg || !swd->write_reg*/ || !fine->init) {
		LOG_DEBUG("no SWD driver?");
		return ERROR_FAIL;
	}

	retval = fine->init();
	if (retval != ERROR_OK) {
		LOG_DEBUG("can't init FINE driver");
		return retval;
	}

	return retval;
}

const char *fine_strerror(int error_code)
{
	switch (error_code) {
	case FINE_CMD_ERR_NOT_SUPPORTED:
		return "command not supported";
	case FINE_CMD_ERR_PACKET:
		return "packet error";
	case FINE_CMD_ERR_CHECKSUM:
		return "checksum error";
	case FINE_CMD_ERR_FLOW:
		return "flow error";
	case FINE_CMD_ERR_ADDRESS:
		return "bad address";
	case FINE_CMD_ERR_INPUT_FREQU:
		return "bad input frequency";
	case FINE_CMD_ERR_SYS_CLK:
		return "bad system clock frequency";
	case FINE_CMD_ERR_AREA:
		return "area error";
	case FINE_CMD_ERR_BIT_RATE:
		return "bit rate error";
	case FINE_CMD_ERR_ENDIAN:
		return "endian error";
	case FINE_CMD_ERR_PROTECTION:
		return "protection error";
	case FINE_CMD_ERR_WRONG_ID:
		return "ID code mismatch error";
	case FINE_CMD_ERR_SERIAL_CNX:
		return "serial programmer connection prohibition error";
	case FINE_CMD_ERR_NON_BLANK:
		return "non-blank error";
	case FINE_CMD_ERR_ERASE:
		return "error of erasure";
	case FINE_CMD_ERR_PROGRAM:
		return "program error";
	case FINE_CMD_ERR_FLASH_SEQ:
		return "flash sequencer error";
	default:
		return "unknown error";
	}
}

static int fine_get_chip_id(const struct fine_driver *fine)
{
	uint8_t out[16];
	uint8_t in[8];
	uint8_t expected[8];

	int ret;
	int retry = 0;

	buf_set_u32_be(out, 0, FINE_START_SEQ);

	memset(in, 0, 2);
	buf_set_u32(expected, 0, 16, 0x0223);

	/* When target receive FINE_START_SEQ, it responds 0x23 0x02 */
	while ((retry < FINE_RETRY_ID_COUNT) && memcmp(in, expected, 2)) {
		ret = fine->xfer(out, in, 4, 2, FINE_TIMEOUT);
		if (ret != ERROR_OK) {
			LOG_ERROR("Error during FINE xfer");
			return ret;
		}
		retry++;
	}

	if (retry == FINE_RETRY_ID_COUNT) {
		LOG_ERROR("Couldn't init FINE");
		return ERROR_FAIL;
	}

	out[0] = FINE_GET_CHIP_ID;
	ret = fine->xfer(out, in, 1, 2, FINE_TIMEOUT);
	if (ret != ERROR_OK) {
		LOG_ERROR("Error during FINE xfer");
		return ret;
	}

	buf_bswap16(in, in, 2);
	LOG_INFO("Found chip id %s", buf_to_hex_str(in, 16));

	return ERROR_OK;
}

static int fine_init_chip(const struct fine_driver *fine)
{
	uint8_t out[16];
	uint8_t in[8];

	int ret;
	int retry = 0;

	out[0] = 0x88;
	out[1] = 0x01;
	out[2] = 0x00;
	out[3] = 0x88;
	out[4] = 0x03;
	out[5] = 0x00;
	out[6] = 0x88;
	out[7] = 0x02;
	out[8] = 0x00;
	out[9] = FINE_ASK_TARGET_ACK;

	ret = fine->xfer(out, in, 10, 2, FINE_TIMEOUT);
	if (ret != ERROR_OK) {
		LOG_ERROR("Error during FINE xfer");
		return ret;
	}

	/* Target responds 0x2000 after power-up, 0x0000 after that */
/*	buf_set_u32(expected, 0, 16, 0x0000);
	if (memcmp(expected, in, 2)) {
		buf_set_u32(expected, 0, 16, 0x0020);
		if (memcmp(expected, in, 2)) {
			printf("Error during FINE initialization");
			return ERROR_FAIL;
		}
	}
*/
	out[0] = 0x84;
	out[1] = 0x55;
	out[2] = 0x00;
	out[3] = 0x00;
	out[4] = 0x00;

	ret = fine->xfer(out, in, 5, 1, FINE_TIMEOUT);
	if (ret != ERROR_OK) {
		LOG_ERROR("Error during FINE xfer");
		return ret;
	}

	if (in[0]) {
		LOG_ERROR("Error during FINE initialization");
		return ERROR_FAIL;
	}

	out[0] = FINE_ASK_TARGET_DATA;
	in[0] = 0x0E;
	while ((in[0] == 0x0E) && (retry < 100)) {
		ret = fine->xfer(out, in, 1, 1, 0x64);
		if (ret != JAYLINK_OK) {
			printf("jaylink_fine_io failed: %s", jaylink_strerror(ret));
			return ERROR_FAIL;
		}
		retry++;
		usleep(10000);
	}

	if (retry == 100) {
		LOG_ERROR("FINE: device timeout");
		return ERROR_FAIL;
	}

	out[0] = FINE_ASK_TARGET_ACK;
	ret = fine->xfer(out, in, 1, 2, 0x64);
	if (ret != JAYLINK_OK) {
		printf("jaylink_fine_io failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	if (in[0] || in[1])
		return ERROR_FAIL;

	return ERROR_OK;
}

static int fine_send_cmd_ack(const struct fine_driver *fine)
{
	uint8_t out = FINE_ASK_TARGET_ACK;
	uint8_t in[2];
	int ret;

	ret = fine->xfer(&out, in, 1, 2, 0x64);
	if (ret != JAYLINK_OK) {
		printf("fine_send_cmd_continue failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	if (in[0] || in[1])
		return ERROR_FAIL;

	return ERROR_OK;
}

static int fine_send_cmd(const struct fine_driver *fine, uint8_t cmd_status, uint8_t cmd, uint8_t *data, uint16_t data_len)
{
	uint8_t out[5];
	uint8_t in[2];
	int idx = 0;
	int ret;

	out[0] = 0x84;
	out[1] = FINE_CMD_SOH | cmd_status;
	out[2] = (data_len + 1) >> 8;
	out[3] = (data_len + 1) & 0xFF;
	out[4] = cmd;

	uint8_t crc = 0;
	for (int i = 0; i < 3; i++)
		crc += out[2 + i];
	for (int i = 0; i < data_len; i++)
		crc += data[i];
	crc = ~crc + 1;

	ret = fine->xfer(out, in, 5, 1, 0x64);
	if (ret != JAYLINK_OK) {
		printf("jaylink_fine_io failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	fine_send_cmd_ack(fine);

	while (data_len > 3) {
		memcpy(&out[1], &data[idx], 4);
		ret = fine->xfer(out, in, 5, 1, 0x64);
		if (ret != JAYLINK_OK) {
			printf("jaylink_fine_io failed: %s", jaylink_strerror(ret));
			return ERROR_FAIL;
		}

		idx += 4;
		data_len -= 4;

		fine_send_cmd_ack(fine);
	}

	memset(&out[1], 0, 4);
	switch (data_len) {
	case 0:
		out[1] = crc;
		out[2] = FINE_CMD_ETX;
		break;
	case 1:
		out[1] = data[idx];
		out[2] = crc;
		out[3] = FINE_CMD_ETX;
		break;
	case 2:
		out[1] = data[idx];
		out[2] = data[idx + 1];
		out[3] = crc;
		out[4] = FINE_CMD_ETX;
		break;
	case 3:
		out[1] = data[idx];
		out[2] = data[idx + 1];
		out[3] = data[idx + 2];
		out[4] = crc;
		break;
	}

	ret = fine->xfer(out, in, 5, 1, 0x64);

	if (data_len == 3) {
		fine_send_cmd_ack(fine);
		out[1] = FINE_CMD_ETX;
		out[2] = 0;
		out[3] = 0;
		out[4] = 0;
		ret = fine->xfer(out, in, 5, 1, 0x64);
	}

	return ERROR_OK;
}

static int fine_get_status_packet(const struct fine_driver *fine)
{
	uint8_t out[5];
	uint8_t in[5];
	uint8_t status[8];

	int ret;

	out[0] = FINE_ASK_TARGET_DATA;
	ret = fine->xfer(out, in, 1, 5, 0x64);
	if (ret != JAYLINK_OK) {
		printf("fine_get_status_packet failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	memcpy(status, &in[1], 4);

	out[0] = FINE_ASK_TARGET_DATA;
	ret = fine->xfer(out, in, 1, 5, 0x64);
	if (ret != JAYLINK_OK) {
		printf("fine_get_status_packet failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	memcpy(&status[4], &in[1], 4);

	ret = fine_send_cmd_ack(fine);
	if (ret != ERROR_OK)
		return ret;

	if (!(status[3] & 0x80))
		return ERROR_OK;

	return status[4];
}

static int fine_get_data(const struct fine_driver *fine, uint8_t *buffer)
{
	uint8_t out[5];
	uint8_t in[5];
	int nb_xfer;
	int ret;

	out[0] = FINE_ASK_TARGET_DATA;
	ret = fine->xfer(out, in, 1, 5, 0x64);
	if (ret != JAYLINK_OK) {
		printf("fine_get_status_packet failed: %s", jaylink_strerror(ret));
		return ERROR_FAIL;
	}

	memcpy(buffer, &in[1], 4);

	int len = 4;

	nb_xfer = ((in[2] << 8) + in[3] + 1) / 4;
	if ((in[3] + 1) % 4)
		nb_xfer++;

	for (int i = 0; i < nb_xfer; i++) {
		out[0] = FINE_ASK_TARGET_DATA;
		ret = fine->xfer(out, in, 1, 5, 0x64);
		if (ret != JAYLINK_OK) {
			printf("fine_get_status_packet failed: %s", jaylink_strerror(ret));
			return ERROR_FAIL;
		}

		memcpy(&buffer[4 + (i * 4)], &in[1], 4);

		len += 4;
	}

	fine_send_cmd_ack(fine);

	return len;
}

static int fine_get_device_type(const struct fine_driver *fine, struct rx *rx_device)
{
	int ret;
	uint8_t buff[200];

	LOG_DEBUG("FINE: Get device type");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_GET_DEVICE_TYPE, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE error: %s", fine_strerror(ret));
		return ret;
	}

	ret = fine_send_cmd(fine, PKT_STATUS, FINE_CMD_GET_DEVICE_TYPE, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	if (fine_get_data(fine, buff) < 0)
		return ERROR_FAIL;

	memcpy(rx_device->type, &buff[4], 8);
	rx_device->max_input_clk_freq  = buf_get_u32_be(buff, 12);
	rx_device->min_input_clk_freq  = buf_get_u32_be(buff, 16);
	rx_device->max_sys_clk_freq    = buf_get_u32_be(buff, 20);
	rx_device->min_sys_clk_freq    = buf_get_u32_be(buff, 24);

	return ERROR_OK;
}

static int fine_set_endianness(const struct fine_driver *fine, int endianness)
{
	uint8_t val = endianness == TARGET_LITTLE_ENDIAN ? 1 : 0;
	int ret;

	LOG_DEBUG("FINE: Set endianness");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_SET_ENDIANNSESS, &val, 1);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	return ERROR_OK;
}

static int fine_set_frequency(const struct fine_driver *fine, int in_freq,
			      int sys_freq, struct rx *rx_device)
{
	uint8_t buff[100];
	uint8_t out[8];
	int ret;

	LOG_DEBUG("FINE: Set frequency");

	buf_set_u32_be(out, 0, in_freq * 1000000);
	buf_set_u32_be(out, 4, sys_freq * 1000000);

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_SET_FREQUENCY, out, 8);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	ret = fine_send_cmd(fine, PKT_STATUS, FINE_CMD_SET_FREQUENCY, NULL, 0);
	if (ret != ERROR_OK)
		return ret;


	if (fine_get_data(fine, buff) < 0)
		return ERROR_FAIL;

	rx_device->sys_clk_freq = buf_get_u32_be(buff, 4);
	rx_device->periph_clk_freq = buf_get_u32_be(buff, 8);

	return ERROR_OK;
}

static int fine_set_bitrate(const struct fine_driver *fine, int bitrate)
{
	uint8_t out[4];
	int ret;

	LOG_DEBUG("FINE: Set bitrate");

	buf_set_u32_be(out, 0, bitrate);

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_SET_BITRATE, out, 4);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	return ERROR_OK;
}

static int fine_send_sync(const struct fine_driver *fine)
{
	int ret;

	LOG_DEBUG("FINE: Send sync");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_SYNC, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	return ERROR_OK;
}

static int fine_get_serial_protect_state(const struct fine_driver *fine, struct rx *rx_device)
{
	uint8_t buff[8];
	int ret;

	LOG_DEBUG("FINE: Get authentication mode");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_GET_AUTH_MODE, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	ret = fine_send_cmd(fine, PKT_STATUS, FINE_CMD_GET_AUTH_MODE, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	if (fine_get_data(fine, buff) < 0)
		return ERROR_FAIL;

	rx_device->serial_allowed = buff[4] ? 0 : 1;

	return ERROR_OK;
}

static int fine_check_id_code(const struct fine_driver *fine, uint8_t *id)
{
	int ret;

	LOG_DEBUG("FINE: Check ID code");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_CHECK_ID_CODE, id, 16);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	return ERROR_OK;
}

static int fine_get_device_mem_info(const struct fine_driver *fine, struct rx *rx_device)
{
	uint8_t buff[24];
	int ret;

	LOG_DEBUG("FINE: Get area informations");

	ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_GET_AREA_COUNT, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_status_packet(fine);
	if (ret != ERROR_OK) {
		LOG_ERROR("FINE command error: %s", fine_strerror(ret));
		return ret;
	}

	ret = fine_send_cmd(fine, PKT_STATUS, FINE_CMD_GET_AREA_COUNT, NULL, 0);
	if (ret != ERROR_OK)
		return ret;

	if (fine_get_data(fine, buff) < 0)
		return ERROR_FAIL;

	rx_device->area_count = buff[4];

	for (uint8_t i = 0; i < rx_device->area_count; i++) {
		LOG_DEBUG("FINE: Area %d Information Acquisition Command", i);

		ret = fine_send_cmd(fine, PKT_CMD, FINE_CMD_GET_AREA_INFO, &i, 1);
		if (ret != ERROR_OK)
			return ret;

		ret = fine_get_status_packet(fine);
		if (ret != ERROR_OK) {
			LOG_ERROR("FINE command error: %s", fine_strerror(ret));
			return ret;
		}

		ret = fine_send_cmd(fine, PKT_STATUS, FINE_CMD_GET_AREA_INFO, NULL, 0);
		if (ret != ERROR_OK)
			return ret;

		if (fine_get_data(fine, buff) < 0)
			return ERROR_FAIL;

		rx_device->area[i].koa = buff[4];
		rx_device->area[i].sad = buf_get_u32_be(buff, 5);
		rx_device->area[i].ead = buf_get_u32_be(buff, 9);
		rx_device->area[i].eau = buf_get_u32_be(buff, 13);
		rx_device->area[i].wau = buf_get_u32_be(buff, 17);
	}

	return ERROR_OK;
}

static int fine_transport_init(struct command_context *cmd_ctx)
{
	const struct fine_driver *fine = adapter_driver->fine_ops;
	struct target *t = get_current_target(cmd_ctx);
	struct rx *rx_device = target_to_rx(t);
	int ret;

	fine->srst();

	ret = fine_get_chip_id(fine);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_init_chip(fine);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_device_type(fine, rx_device);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_set_endianness(fine, TARGET_LITTLE_ENDIAN);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_set_frequency(fine, 16, 120, rx_device);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_set_bitrate(fine, 1000000);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_send_sync(fine);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_serial_protect_state(fine, rx_device);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_check_id_code(fine, rx_device->id_code);
	if (ret != ERROR_OK)
		return ret;

	ret = fine_get_device_mem_info(fine, rx_device);
	if (ret != ERROR_OK)
		return ret;

	return ERROR_OK;
}

static struct transport fine_transport = {
	.name = "fine",
	.select = fine_transport_select,
	.init = fine_transport_init,
};

static void fine_constructor(void) __attribute__ ((constructor));
static void fine_constructor(void)
{
	transport_register(&fine_transport);
}

bool transport_is_fine(void)
{
	return get_current_transport() == &fine_transport;
}
