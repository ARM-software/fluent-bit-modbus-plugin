/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Fluent Bit Modbus Plugin
 * ========================
 * Copyright (C) 2019  ARM Limited, All Rights Reserved
 *
 * This file is part of Fluent Bit Modbus Plugin.
 *
 * Fluent Bit Modbus Plugin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Fluent Bit Modbus Plugin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Fluent Bit Modbus Plugin. If not, see https://www.gnu.org/licenses/.
 *
*/

#ifndef FLB_OUT_MODBUS_H
#define FLB_OUT_MODBUS_H

#include <modbus.h>

struct flb_out_modbus_config {
    modbus_t *modbus_ctx;
    int err;
};

#endif
