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
#include <fluent-bit/flb_output.h>
#include <fluent-bit/flb_pack.h>

#include "out_modbus.h"

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

enum {
    ADDRESS = 0,
    VALUE
};

char *type_str[4] = {
    "coils",
    "discrete_inputs",
    "holding_registers",
    "input_registers"
};

char *key_str[2] = {
    "address",
    "value"
};

int key_compare(char *key, const char *str, int size)
{
    if (strlen(key) == size) {
        if (strncmp(key, str, size) == 0) {
            return 0;
        }
    }

    return -1;
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

int out_modbus_connect(struct flb_out_modbus_config *ctx)
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

static int configure(struct flb_out_modbus_config *ctx,
                     struct flb_output_instance *in)
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

    /* Initializing Modbus connection */
    str = flb_output_get_property("backend", in);
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
            flb_error("[out_modbus] Backend %s unknown: has to be [tcp|tcppi|rtu]",
                      str);
            return -1;
        }
    }
    else {
        use_backend = TCP;
    }

    /* Modbus slave (server) information */
    addr = flb_output_get_property("address", in);
    port = flb_output_get_property("tcp_port", in);
    rate = flb_output_get_property("rate", in);

    switch (use_backend) {
    case TCP:
        if (addr == NULL) {
            flb_error("[out_modbus] Slave (tcp) address %s unknown");
            return -1;
        }
        if (port == NULL) {
            port = "502";
        }
        modbus_ctx = modbus_new_tcp(addr, atoi(port));
        break;
    case TCP_PI:
        if (addr == NULL) {
            flb_error("[out_modbus] Slave (tcppi) address %s unknown");
            return -1;
        }
        if (port == NULL) {
            port = "502";
        }
        modbus_ctx = modbus_new_tcp_pi(addr, port);
        break;
    default:
        if (addr == NULL) {
            flb_error("[out_modbus] Slave (rtu) device %s unknown");
            return -1;
        }
        if (rate == NULL) {
            flb_error("[out_modbus] Connection rate %s unknown");
            return -1;
        }
        modbus_ctx = modbus_new_rtu(addr, atoi(rate), 'N', 8, 1);
        break;
    }

    if (modbus_ctx == NULL) {
        flb_error("[out_modbus] Unable to allocate modbus context");
        return -1;
    }

    //modbus_set_debug(modbus_ctx, TRUE);
    modbus_set_error_recovery(modbus_ctx,
                              MODBUS_ERROR_RECOVERY_PROTOCOL);

    ctx->modbus_ctx = modbus_ctx;

    if (out_modbus_connect(ctx) == -1) {
        return -1;
    }

    return 0;
}

static void config_destroy(struct flb_out_modbus_config *ctx)
{
    modbus_free(ctx->modbus_ctx);
    flb_free(ctx);
}

static int out_modbus_init(struct flb_output_instance *in,
                           struct flb_config *config, void *data)
{
    int ret;

    struct flb_out_modbus_config *ctx = NULL;

    ctx = flb_malloc(sizeof(struct flb_out_modbus_config));
    if (ctx == NULL) {
        return -1;
    }

    /* Initialize head config */
    ret = configure(ctx, in);
    if (ret < 0) {
        config_destroy(ctx);
        return -1;
    }

    flb_output_set_context(in, ctx);

    return 0;
}

static void out_modbus_flush(const void *data, size_t bytes,
                             const char *tag, int tag_len,
                             struct flb_input_instance *i_ins,
                             void *out_context,
                             struct flb_config *config)
{
    int i;
    int idata;
    int ikey;
    int rc;
    int *addr = NULL;
    uint16_t *value = NULL;
    int map_size;
    int data_size;
    size_t off = 0;
    msgpack_object root;
    msgpack_object map;
    msgpack_object key;
    msgpack_object val;
    /* data per */
    msgpack_object data_map;
    msgpack_object data_key;
    msgpack_object data_val;
    msgpack_unpacked result;
    struct flb_out_modbus_config *ctx = out_context;

    /* If last call received connection error, try to reconnect */
    if (connection_error(ctx->err)) {
        modbus_close(ctx->modbus_ctx);
        if (out_modbus_connect(ctx) == -1) {
            FLB_OUTPUT_RETURN(FLB_RETRY);
            return;
        }
    }

    msgpack_unpacked_init(&result);
    while (msgpack_unpack_next(&result, data, bytes, &off) == MSGPACK_UNPACK_SUCCESS) {
        root = result.data;

        /* get the map data and it size (number of items) */
        map   = root.via.array.ptr[1];
        map_size = map.via.map.size;

        for (i = 0; i < map_size; i++) {
            key = map.via.map.ptr[i].key;
            val = map.via.map.ptr[i].val;

            /* [{index: i, value: v}, ...] */
            if (val.type != MSGPACK_OBJECT_ARRAY) {
                continue;
            }

            data = val.via.array.ptr;
            data_size = val.via.map.size;

            for (ikey = 0; ikey < val.via.array.size; ikey++) {
                /* {index: ind, value: val} */
                data_map = val.via.array.ptr[ikey];

                if (data_map.type != MSGPACK_OBJECT_MAP) {
                    break;
                }

                data_size = data_map.via.map.size;

                for (idata = 0; idata < data_size; idata++) {
                    data_key = data_map.via.map.ptr[idata].key;
                    data_val = data_map.via.map.ptr[idata].val;

                    if (key_compare(key_str[ADDRESS], data_key.via.str.ptr,
                                    data_key.via.str.size) == 0) {
                        if (data_val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                            addr = flb_malloc(sizeof(int));
                            *addr = data_val.via.i64;
                            continue;
                        }
                    }

                    if (key_compare(key_str[VALUE], data_key.via.str.ptr,
                                    data_key.via.str.size) == 0) {
                        if (data_val.type == MSGPACK_OBJECT_POSITIVE_INTEGER ||
                            data_val.type == MSGPACK_OBJECT_NEGATIVE_INTEGER) {
                            value = flb_malloc(sizeof(uint16_t));
                            *value = data_val.via.i64;
                            continue;
                        }
                        else if (data_val.type == MSGPACK_OBJECT_BOOLEAN) {
                            value = flb_malloc(sizeof(uint16_t));
                            *value = (uint16_t) data_val.via.boolean;
                            continue;
                        }
                    }
                }

                if (addr != NULL && value != NULL) {
                    /* Coils */
                    if (key_compare(type_str[COILS], key.via.str.ptr,
                                    key.via.str.size) == 0) {
                        rc = modbus_write_bit(ctx->modbus_ctx, *addr, (bool) *value);
                        if (rc != 1) {
                            flb_error("Error writing to coil at address = %d, value = %d: %s\n",
                                      addr, value, modbus_strerror(errno));
                        }
                    }
                    else if (key_compare(type_str[HOLDING_REGISTERS], key.via.str.ptr,
                                         key.via.str.size) == 0) {
                        rc = modbus_write_register(ctx->modbus_ctx, *addr, *value);
                        if (rc != 1) {
                            flb_error("Error writing to register at address = %d, value = %d: %s\n",
                                      addr, value, modbus_strerror(errno));
                        }
                    }
                }

                if (addr) {
                    flb_free(addr);
                }
                if (value) {
                    flb_free(value);
                }
            }
        }
    }
    msgpack_unpacked_destroy(&result);

    FLB_OUTPUT_RETURN(FLB_OK);
}

static int out_modbus_exit(void *data, struct flb_config *config)
{
    struct flb_out_modbus_config *ctx = data;
    if (!ctx) {
        return 0;
    }

    config_destroy(ctx);

    return 0;
}

struct flb_output_plugin out_modbus_plugin = {
    .name         = "modbus",
    .description  = "Modbus output plugin",
    .cb_init      = out_modbus_init,
    .cb_flush     = out_modbus_flush,
    .cb_exit      = out_modbus_exit,
    .flags        = 0,
};
