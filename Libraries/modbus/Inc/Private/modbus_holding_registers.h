#ifndef MODBUS_HOLDING_REGISTERS_H
#define MODBUS_HOLDING_REGISTERS_H

#include <stddef.h>
#include <stdint.h>

#include "modbus_types.h"

modbus_dispatch_result_t dispatch_read_holding_registers(modbus_context_t *context,
                                                         const uint8_t *wire_request,
                                                         uint8_t request_length,
                                                         uint8_t *wire_response,
                                                         uint8_t response_capacity);

modbus_dispatch_result_t dispatch_write_holding_registers(modbus_context_t *context,
                                                          const uint8_t *wire_request,
                                                          uint8_t request_length,
                                                          uint8_t *wire_response,
                                                          uint8_t response_capacity);

modbus_dispatch_result_t dispatch_write_multiple_holding_registers(modbus_context_t *context,
                                                                   const uint8_t *wire_request,
                                                                   uint8_t request_length,
                                                                   uint8_t *wire_response,
                                                                   uint8_t response_capacity);

#endif // MODBUS_HOLDING_REGISTERS_H
