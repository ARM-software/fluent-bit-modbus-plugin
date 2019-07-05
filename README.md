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

Assuming that `$FLUENTBIT_DIR` and `$LIBMODBUS_DIR` store absolute locations of Fluent Bit and libmodbus, run the following in order to create Modbus shared library:

```bash
$ cmake -DFLB_SOURCE=$FLUENTBIT_DIR -DMODBUS_SOURCE=$LIBMODBUS_DIR -DPLUGIN_NAME=in_modbus ../
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
