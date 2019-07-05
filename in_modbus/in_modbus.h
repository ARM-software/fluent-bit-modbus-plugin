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

#ifndef FLB_IN_MODBUS_H
#define FLB_IN_MODBUS_H

#include <modbus.h>

struct flb_in_modbus_config {
	/* nop stands for Number of Points */
	modbus_t *modbus_ctx;

	int time_interval_sec;

	int coil_addr;
	int coil_nop;

	int discrete_input_addr;
	int discrete_input_nop;

	int holding_reg_addr;
	int holding_reg_nop;

	int input_reg_addr;
	int input_reg_nop;
};

#endif
