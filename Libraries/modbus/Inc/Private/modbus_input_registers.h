#ifndef MODBUS_INPUT_REGISTERS_H
#define MODBUS_INPUT_REGISTERS_H

#include <stddef.h>
#include <stdint.h>

#include "modbus_types.h"

modbus_dispatch_result_t dispatch_read_input_registers(modbus_context_t *context,
                                                       const uint8_t *wire_request,
                                                       uint8_t request_length,
                                                       uint8_t *wire_response,
                                                       size_t response_capacity);

#endif // MODBUS_INPUT_REGISTERS_H
