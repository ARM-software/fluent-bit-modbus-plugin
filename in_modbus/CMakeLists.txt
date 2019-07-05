set(CMAKE_MACOSX_RPATH 1)

set(src
  in_modbus.c
  )

include_directories(${MODBUS_SRC}/src)
link_directories(${MODBUS_SRC}/src/.libs)

FLB_PLUGIN(in_modbus "${src}" "modbus")

