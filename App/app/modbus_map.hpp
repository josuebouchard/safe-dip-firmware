#ifndef MODBUS_MAP_HPP
#define MODBUS_MAP_HPP

#include "machine.hpp"
#include "modbus_types.h"

// Returns a context that serves the register map from `machine`. The Modbus task
// stores a pointer to it, so the caller must keep it alive as long as the task.
modbus_context_t modbus_map_make_context(Machine &machine) noexcept;

#endif // MODBUS_MAP_HPP
