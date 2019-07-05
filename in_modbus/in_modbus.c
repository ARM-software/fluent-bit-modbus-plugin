/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019      The Fluent Bit Authors
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_pack.h>
#include <modbus.h>

#include "in_modbus.h"

enum {
	TCP,
	TCP_PI,
	RTU
};

void pack_error(msgpack_packer *mp_pck)
{
	const char *error = modbus_strerror(errno);

	msgpack_pack_map(mp_pck, 1);
	msgpack_pack_str(mp_pck, strlen("error"));
	msgpack_pack_str_body(mp_pck, "error", strlen("error"));
	msgpack_pack_str(mp_pck, strlen(error));
	msgpack_pack_str_body(mp_pck, error, strlen(error));
}

/* collect callback */
static int in_modbus_collect(struct flb_input_instance *i_ins,
                             struct flb_config *config, void *in_context)
{
	struct flb_in_modbus_config *ctx = in_context;
	msgpack_packer mp_pck;
	msgpack_sbuffer mp_sbuf;

	int i;
	int ret;
	int map_entries;
	int nbits;
	int nbytes;
	uint8_t *bits;
	uint16_t *registers;

	map_entries = (ctx->coil_nop > 0) +
	              (ctx->discrete_input_nop > 0) +
	              (ctx->holding_reg_nop > 0) +
	              (ctx->input_reg_nop > 0);

	/* Initializing Message Pack */
	msgpack_sbuffer_init(&mp_sbuf);
	msgpack_packer_init(&mp_pck, &mp_sbuf, msgpack_sbuffer_write);

	msgpack_pack_array(&mp_pck, 2);
	flb_pack_time_now(&mp_pck);

	/* Coils, Discrete Inputs */
	msgpack_pack_map(&mp_pck, map_entries);

	nbits = ctx->coil_nop > ctx->discrete_input_nop ?
	        ctx->coil_nop : ctx->discrete_input_nop;
	bits = (uint8_t *) flb_calloc(nbits, sizeof(uint8_t));
	if (!bits) {
		flb_errno();
		return -1;
	}

	/* Read coil values */
	if (ctx->coil_nop > 0) {
		ret = modbus_read_bits(ctx->modbus_ctx,
		                       ctx->coil_addr,
		                       ctx->coil_nop, bits);

		msgpack_pack_str(&mp_pck, strlen("coils"));
		msgpack_pack_str_body(&mp_pck, "coils", strlen("coils"));

		if (ret == -1) {
			/* Error */
			pack_error(&mp_pck);
		}
		else {
			msgpack_pack_array(&mp_pck, ret);

			for (i = 0; i < ret; i++)
			{
				msgpack_pack_uint8(&mp_pck, bits[i]);
			}
		}
	}

	/* Read input bits values */
	if (ctx->discrete_input_nop > 0) {
		ret = modbus_read_input_bits(ctx->modbus_ctx,
		                             ctx->discrete_input_addr,
		                             ctx->discrete_input_nop, bits);

		msgpack_pack_str(&mp_pck, strlen("discrete_inputs"));
		msgpack_pack_str_body(&mp_pck, "discrete_inputs", strlen("discrete_inputs"));

		if (ret == -1) {
			/* Error */
			pack_error(&mp_pck);
		}
		else {
			msgpack_pack_array(&mp_pck, ret);

			for (i = 0; i < ret; i++)
			{
				msgpack_pack_uint8(&mp_pck, bits[i]);
			}
		}
	}

	nbytes = ctx->holding_reg_nop > ctx->input_reg_nop ?
	         ctx->holding_reg_nop : ctx->input_reg_nop;
	registers = (uint16_t *) flb_calloc(nbytes, sizeof(uint16_t));
	if (!bits) {
		flb_errno();
		return -1;
	}

	/* Read register values */
	if (ctx->holding_reg_nop > 0) {
		ret = modbus_read_registers(ctx->modbus_ctx,
		                            ctx->holding_reg_addr,
		                            ctx->holding_reg_nop, registers);

		msgpack_pack_str(&mp_pck, strlen("holding_registers"));
		msgpack_pack_str_body(&mp_pck, "holding_registers",
		                      strlen("holding_registers"));

		if (ret == -1) {
			/* Error */
			pack_error(&mp_pck);
		}
		else {
			msgpack_pack_array(&mp_pck, ret);

			for (i = 0; i < ret; i++)
			{
				msgpack_pack_uint16(&mp_pck, registers[i]);
			}
		}

	}

	/* Read input register values */
	if (ctx->input_reg_nop > 0) {
		ret = modbus_read_input_registers(ctx->modbus_ctx,
		                                  ctx->input_reg_addr,
		                                  ctx->input_reg_nop, registers);

		msgpack_pack_str(&mp_pck, strlen("input_registers"));
		msgpack_pack_str_body(&mp_pck, "input_registers",
		                      strlen("input_registers"));

		if (ret == -1) {
			/* Error */
			pack_error(&mp_pck);
		}
		else {
			msgpack_pack_array(&mp_pck, ret);

			for (i = 0; i < ret; i++)
			{
				msgpack_pack_uint16(&mp_pck, registers[i]);
			}
		}
	}

	flb_input_chunk_append_raw(i_ins, NULL, 0, mp_sbuf.data, mp_sbuf.size);
	msgpack_sbuffer_destroy(&mp_sbuf);

	flb_free(bits);
	flb_free(registers);

	return 0;
}

int value_from_cfg(struct flb_input_instance *in, char *key, int def)
{
	const char *str;

	str = flb_input_get_property(key, in);
	if (str != NULL) {
		return atoi(str);
	}
	else {
		return def;
	}
}

static int configure(struct flb_in_modbus_config *ctx,
                     struct flb_input_instance *in)
{
	const char *str;
	const char *addr;
	const char *port;
	const char *rate;
	const char *tid;
	int use_backend;
	modbus_t *modbus_ctx;

	modbus_ctx = NULL;

	/* Interval settings (1 sec default scan) */
	ctx->time_interval_sec = value_from_cfg(in, "time_interval", 1);

	/* Modbus coils to scan */
	ctx->coil_addr = value_from_cfg(in, "coil_addr", 0);
	ctx->coil_nop = value_from_cfg(in, "coil_nop", 0);

	/* Modbus discrete inputs to scan */
	ctx->discrete_input_addr = value_from_cfg(in, "discrete_input_addr", 0);
	ctx->discrete_input_nop = value_from_cfg(in, "discrete_input_nop", 0);

	/* Modbus holding registers to scan */
	ctx->holding_reg_addr = value_from_cfg(in, "holding_reg_addr", 0);
	ctx->holding_reg_nop = value_from_cfg(in, "holding_reg_nop", 0);

	/* Modbus input registers to scan */
	ctx->input_reg_addr = value_from_cfg(in, "input_reg_addr", 0);
	ctx->input_reg_nop = value_from_cfg(in, "input_reg_nop", 0);

	/* Initializing Modbus connection */
	str = flb_input_get_property("backend", in);
	if (str != NULL) {
		if (strcmp(str, "tcp") == 0) {
			use_backend = TCP;
		}
		else if (strcmp(str, "tcppi") == 0) {
			use_backend = TCP_PI;
		}
		else if (strcmp(str, "rtu") == 0) {
			use_backend = RTU;
		}
		else {
			flb_error("[in_modbus] Backend %s unknown: has to be [tcp|tcppi|rtu]",
			          str);
			return -1;
		}
	}
	else {
		use_backend = TCP;
	}

	/* Modbus slave (server) information */
	addr = flb_input_get_property("address", in);
	port = flb_input_get_property("tcp_port", in);
	rate = flb_input_get_property("rate", in);

	switch (use_backend) {
	case TCP:
		if (addr == NULL) {
			flb_error("[in_modbus] Slave (tcp) address %s unknown");
			return -1;
		}
		if (port == NULL) {
			port = "502";
		}
		modbus_ctx = modbus_new_tcp(addr, atoi(port));
		break;
	case TCP_PI:
		if (addr == NULL) {
			flb_error("[in_modbus] Slave (tcppi) address %s unknown");
			return -1;
		}
		if (port == NULL) {
			port = "502";
		}
		modbus_ctx = modbus_new_tcp_pi(addr, port);
		break;
	default:
		if (addr == NULL) {
			flb_error("[in_modbus] Slave (rtu) device %s unknown");
			return -1;
		}
		if (rate == NULL) {
			flb_error("[in_modbus] Connection rate %s unknown");
			return -1;
		}
		modbus_ctx = modbus_new_rtu(addr, atoi(rate), 'N', 8, 1);
		break;
	}

	if (modbus_ctx == NULL) {
		flb_error("[in_modbus] Unable to allocate modbus context");
		return -1;
	}

	//modbus_set_debug(modbus_ctx, TRUE);
	modbus_set_error_recovery(modbus_ctx,
	                          MODBUS_ERROR_RECOVERY_LINK |
	                          MODBUS_ERROR_RECOVERY_PROTOCOL);

	if (modbus_connect(modbus_ctx) == -1) {
		flb_error("Connection to address %s and port %s failed: %s\n",
		          addr, port, modbus_strerror(errno));
		modbus_free(modbus_ctx);
		return -1;
	}

	ctx->modbus_ctx = modbus_ctx;

	return 0;
}

static int config_destroy(struct flb_in_modbus_config *ctx)
{
	modbus_free(ctx->modbus_ctx);
	flb_free(ctx);
	return 0;
}

/* Initialize plugin */
static int in_modbus_init(struct flb_input_instance *in,
                          struct flb_config *config, void *data)
{
	int ret;

	struct flb_in_modbus_config *ctx = NULL;

	ctx = flb_malloc(sizeof(struct flb_in_modbus_config));
	if (ctx == NULL) {
		return -1;
	}

	/* Initialize head config */
	ret = configure(ctx, in);
	if (ret < 0) {
		config_destroy(ctx);
		return -1;
	}

	flb_input_set_context(in, ctx);
	ret = flb_input_set_collector_time(in,
	                                   in_modbus_collect,
	                                   ctx->time_interval_sec,
	                                   0,
	                                   config);

	if (ret < 0) {
		flb_error("could not set collector for dummy input plugin");
		config_destroy(ctx);
		return -1;
	}

	return 0;
}

static int in_modbus_exit(void *data, struct flb_config *config)
{
	return 0;
}

struct flb_input_plugin in_modbus_plugin = {
	.name         = "modbus",
	.description  = "Modbus input plugin",
	.cb_init      = in_modbus_init,
	.cb_pre_run   = NULL,
	.cb_collect   = in_modbus_collect,
	.cb_flush_buf = NULL,
	.cb_exit      = in_modbus_exit
};
