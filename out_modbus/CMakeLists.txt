set(CMAKE_MACOSX_RPATH 1)

set(src
  out_modbus.c
  )

include_directories(${MODBUS_SRC}/src)
link_directories(${MODBUS_SRC}/src/.libs)

FLB_PLUGIN(out_modbus "${src}" "modbus")
