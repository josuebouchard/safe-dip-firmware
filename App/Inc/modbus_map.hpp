#ifndef MODBUS_MAP_HPP
#define MODBUS_MAP_HPP

#include "machine.hpp"
#include "modbus_types.h"

// Binds the register map to `machine` and returns the context for the Modbus task.
// The library's callbacks take no user pointer, so the binding is file-scope.
modbus_context_t *modbus_map_init(Machine &machine) noexcept;

#endif // MODBUS_MAP_HPP
