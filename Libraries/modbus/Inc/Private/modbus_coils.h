#ifndef MODBUS_COILS_H
#define MODBUS_COILS_H

#include <stddef.h>
#include <stdint.h>

#include "modbus_types.h"

modbus_dispatch_result_t dispatch_read_coils(modbus_context_t *context, const uint8_t *wire_request,
                                             uint8_t request_length, uint8_t *wire_response,
                                             uint8_t response_capacity);

modbus_dispatch_result_t dispatch_write_single_coil(modbus_context_t *context,
                                                    const uint8_t *wire_request,
                                                    uint8_t request_length, uint8_t *wire_response,
                                                    uint8_t response_capacity);

modbus_dispatch_result_t dispatch_write_multiple_coils(modbus_context_t *context,
                                                       const uint8_t *wire_request,
                                                       uint8_t request_length,
                                                       uint8_t *wire_response,
                                                       uint8_t response_capacity);

#endif // MODBUS_COILS_H
