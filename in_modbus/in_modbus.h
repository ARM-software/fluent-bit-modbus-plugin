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

#ifndef FLB_IN_MODBUS_H
#define FLB_IN_MODBUS_H

#include <modbus.h>

struct flb_in_modbus_config {
    /* 'no' postfix stands for Number of Points */
    modbus_t *modbus_ctx;
    int err;

    int time_interval_sec;

    int coil_addr;
    int coil_no;

    int discrete_input_addr;
    int discrete_input_no;

    int holding_reg_addr;
    int holding_reg_no;

    int input_reg_addr;
    int input_reg_no;
};

#endif
