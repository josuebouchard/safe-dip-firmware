#ifndef MODBUS_H
#define MODBUS_H

#include "modbus_types.h"
#include <stdint.h>
#include <stddef.h>

// Outputs response/exception length
// If a context function is not implemented, the corresponding instruction will be considered
// not implemented.
modbus_dispatch_result_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                                             uint8_t request_length, uint8_t *wire_response,
                                             size_t response_capacity);

#endif // MODBUS_H
