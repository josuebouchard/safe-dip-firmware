#ifndef MODBUS_DISCRETE_INPUTS_H
#define MODBUS_DISCRETE_INPUTS_H

#include "modbus_types.h"
#include <stddef.h>
#include <stdint.h>

modbus_dispatch_result_t dispatch_read_discrete_inputs(modbus_context_t *context, const uint8_t *wire_request,
                                     uint8_t request_length, uint8_t *wire_response,
                                     size_t response_capacity);

#endif // MODBUS_DISCRETE_INPUTS_H
