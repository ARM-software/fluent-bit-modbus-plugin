/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019  ARM Limited, All Rights Reserved
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
#include <errno.h>
#include <modbus.h>

#include "in_modbus.h"

enum {
    TCP,
    TCP_PI,
    RTU
};

enum {
    COILS = 0,
    DISCRETE_INPUTS,
    HOLDING_REGISTERS,
    INPUT_REGISTERS
};

char *type_str[4] = {
    "coils",
    "discrete_inputs",
    "holding_registers",
    "input_registers"
};

#define BIT_TYPE(type) type == COILS || type == DISCRETE_INPUTS

void pack_error(msgpack_packer *mp_pck)
{
    const char *error = modbus_strerror(errno);

    msgpack_pack_map(mp_pck, 1);
    msgpack_pack_str(mp_pck, strlen("error"));
    msgpack_pack_str_body(mp_pck, "error", strlen("error"));
    msgpack_pack_str(mp_pck, strlen(error));
    msgpack_pack_str_body(mp_pck, error, strlen(error));
}

bool connection_error(int error)
{
    return error == EBADF ||
           error == ECONNRESET ||
           error == EPIPE ||
           error == ECONNREFUSED ||
           error == ETIMEDOUT ||
           error == ENOPROTOOPT ||
           error == EINPROGRESS;
}

int in_modbus_connect(struct flb_in_modbus_config *ctx)
{
    errno = 0;
    if (modbus_connect(ctx->modbus_ctx) == -1) {
        ctx->err = errno;
        flb_error("Connection to Modbus slave failed: %s\n",
                  modbus_strerror(errno));
        return -1;
    }

    ctx->err = 0;
    return 0;
}

int pack_inputs(struct flb_in_modbus_config *ctx, msgpack_packer *mp_pck,
                void *inputs, int type, int num)
{
    int i;

    uint8_t *bits = (uint8_t *) inputs;
    uint16_t *registers = (uint16_t *) inputs;

    ctx->err = 0;

    if (connection_error(errno)) {
        ctx->err = errno;
        flb_error("Connection to Modbus slave failed: %s\n",
                  modbus_strerror(errno));
        return -1;
    }

    msgpack_pack_str(mp_pck, strlen(type_str[type]));
    msgpack_pack_str_body(mp_pck, type_str[type], strlen(type_str[type]));

    if (num == -1) { /* Non-connection error */
        /* Error */
        ctx->err = errno;
        pack_error(mp_pck);
    }
    else {
        msgpack_pack_array(mp_pck, num);

        for (i = 0; i < num; i++)
        {
            if (BIT_TYPE(type)) {
                msgpack_pack_uint8(mp_pck, bits[i]);
            }
            else {
                msgpack_pack_uint16(mp_pck, registers[i]);
            }
        }
    }

    return 0;
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

    /* If last call received connection error, try to reconnect */
    if (connection_error(ctx->err)) {
        modbus_close(ctx->modbus_ctx);
        if (in_modbus_connect(ctx) == -1) {
            return -1;
        }
    }

    map_entries = (ctx->coil_no > 0) +
                  (ctx->discrete_input_no > 0) +
                  (ctx->holding_reg_no > 0) +
                  (ctx->input_reg_no > 0);

    /* Initializing Message Pack */
    msgpack_sbuffer_init(&mp_sbuf);
    msgpack_packer_init(&mp_pck, &mp_sbuf, msgpack_sbuffer_write);

    msgpack_pack_array(&mp_pck, 2);
    flb_pack_time_now(&mp_pck);

    /* Coils, Discrete Inputs */
    msgpack_pack_map(&mp_pck, map_entries);

    nbits = ctx->coil_no > ctx->discrete_input_no ?
            ctx->coil_no : ctx->discrete_input_no;
    bits = (uint8_t *) flb_calloc(nbits, sizeof(uint8_t));
    if (!bits) {
        flb_errno();
        return -1;
    }

    nbytes = ctx->holding_reg_no > ctx->input_reg_no ?
             ctx->holding_reg_no : ctx->input_reg_no;
    registers = (uint16_t *) flb_calloc(nbytes, sizeof(uint16_t));
    if (!registers) {
        flb_errno();
        return -1;
    }

    /* Read coil values */
    if (ctx->coil_no > 0) {
        errno = 0;
        ret = modbus_read_bits(ctx->modbus_ctx,
                               ctx->coil_addr,
                               ctx->coil_no, bits);
        if (pack_inputs(ctx, &mp_pck, bits, COILS, ret) == -1) {
            goto cleanup;
        }
    }

    /* Read input bits values */
    if (ctx->discrete_input_no > 0) {
        errno = 0;
        ret = modbus_read_input_bits(ctx->modbus_ctx,
                                     ctx->discrete_input_addr,
                                     ctx->discrete_input_no, bits);
        if (pack_inputs(ctx, &mp_pck, bits, DISCRETE_INPUTS, ret) == -1) {
            goto cleanup;
        }
    }

    /* Read register values */
    if (ctx->holding_reg_no > 0) {
        errno = 0;
        ret = modbus_read_registers(ctx->modbus_ctx,
                                    ctx->holding_reg_addr,
                                    ctx->holding_reg_no, registers);
        if (pack_inputs(ctx, &mp_pck, registers, HOLDING_REGISTERS, ret) == -1) {
            goto cleanup;
        }
    }

    /* Read input register values */
    if (ctx->input_reg_no > 0) {
        errno = 0;
        ret = modbus_read_input_registers(ctx->modbus_ctx,
                                          ctx->input_reg_addr,
                                          ctx->input_reg_no, registers);
        if (pack_inputs(ctx, &mp_pck, registers, INPUT_REGISTERS, ret) == -1) {
            goto cleanup;
        }
    }

    flb_input_chunk_append_raw(i_ins, NULL, 0, mp_sbuf.data, mp_sbuf.size);

cleanup:
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

    ctx->err = 0;
    /* Interval settings (1 sec default scan) */
    ctx->time_interval_sec = value_from_cfg(in, "time_interval", 1);

    /* Modbus coils to scan */
    ctx->coil_addr = value_from_cfg(in, "coil_addr", 0);
    ctx->coil_no = value_from_cfg(in, "coil_no", 0);

    /* Modbus discrete inputs to scan */
    ctx->discrete_input_addr = value_from_cfg(in, "discrete_input_addr", 0);
    ctx->discrete_input_no = value_from_cfg(in, "discrete_input_no", 0);

    /* Modbus holding registers to scan */
    ctx->holding_reg_addr = value_from_cfg(in, "holding_reg_addr", 0);
    ctx->holding_reg_no = value_from_cfg(in, "holding_reg_no", 0);

    /* Modbus input registers to scan */
    ctx->input_reg_addr = value_from_cfg(in, "input_reg_addr", 0);
    ctx->input_reg_no = value_from_cfg(in, "input_reg_no", 0);

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
                              MODBUS_ERROR_RECOVERY_PROTOCOL);

    ctx->modbus_ctx = modbus_ctx;

    if (in_modbus_connect(ctx) == -1) {
        return -1;
    }

    return 0;
}

static void config_destroy(struct flb_in_modbus_config *ctx)
{
    modbus_free(ctx->modbus_ctx);
    flb_free(ctx);
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
    struct flb_in_modbus_config *ctx = data;
    if (!ctx) {
        return 0;
    }

    config_destroy(ctx);

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
