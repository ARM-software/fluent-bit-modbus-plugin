# Fluent Bit Modbus Plugin

[Modbus](https://en.wikipedia.org/wiki/Modbus) is a well-known communication protocol to connect industrial devices. This repository provides the source code to build Modbus dynamic input plugin to use in Fluent Bit.

## Requirements

- [Fluent Bit](https://fluentbit.io) Source code, version >= 1.2
- [libmodbus](https://github.com/stephane/libmodbus) Source code, version >= v3.1.4
- C compiler: GCC or Clang
- CMake3

## Getting Started

Build the static libarary for `libmodbus` (assuming that `$LIBMODBUS_DIR` stores the absolute location of `libmodbus` directory):

```bash
$ cd $LIBMODBUS_DIR
$ ./autogen.sh
$ ./configure --enable_static
$ make
```

Provide the following variables for the CMake:

- FLB\_SOURCE: absolute path to source code of Fluent Bit.
- MODBUS\_SOURCE: absolute path to source code of libmodbus.
- PLUGIN\_NAME: `in_modbus`

Assuming that `$FLUENTBIT_DIR` and `$LIBMODBUS_DIR` store absolute locations of Fluent Bit and libmodbus, and `$PLUGIN_NAME` is one of `in_modbus` or `out_modbus` plugins, run the following in order to create Modbus shared library:

```bash
$ cmake -DFLB_SOURCE=$FLUENTBIT_DIR -DMODBUS_SRC=$LIBMODBUS_DIR -DPLUGIN_NAME=$PLUGIN_NAME ../
```

then type 'make' to build the plugin:

```
$ make
Scanning dependencies of target flb-in_modbus
[ 50%] Building C object in_modbus/CMakeFiles/flb-in_modbus.dir/in_modbus.c.o
[100%] Linking C shared library ../flb-in_modbus.so
[100%] Built target flb-in_modbus
```

Add the path to created __.so__ file to the plugins configuration file in order to call it from Fluent Bit (see [plugins configuration](https://github.com/fluent/fluent-bit/blob/master/conf/plugins.conf)).

## Usage

### Input plugin

Input modbus plugin reads inputs and outputs in batch, which means, the user needs to provide the start address for each type of input and output, and the number of bits/registers to read from that address. For instance, the following configuration reads 5 elements from each type (coils/discrete inputs/holding registers/input registers) every second:

```
[INPUT]
    Name                modbus
    alias               in_modbus
    time_interval       1
    address             10.1.1.35
    tcp_port            502
    coil_addr           0
    coil_no             5
    discrete_input_addr 30
    discrete_input_no   5
    holding_reg_addr    100
    holding_reg_no      5
    input_reg_addr      200
    input_reg_no        5

```

The output from modbus input plugin is similar to this:

```json
{
    "coils": [0, 1, 0, 0, 0],
    "discrete_inputs": [0, 0, 0, 1, 0],
    "holding_registers": [0, 254, 0, 0, 0],
    "input_registers": [0, 0, 0, 0, 0]
}
```

### Output plugin

Unlike input plugin, Output Modbus plugin writes to coils and holding registers in a discrete way, which means, user has to specify a single address to write to, and its value. Configuration only needs the IP address and the port of the slave, and the match string:

```
[OUTPUT]
    Name                modbus
    Match               mqtt
    address             10.1.1.35
    tcp_port            502
```

And the data format is similar to this, which `address` and `value` denote the parameters required for updating a single element:

```json
{
    "coils": [{"address": 0, "value": 1}],
    "holding_registers": [{"address": 200, "value": 254}]
}
```
