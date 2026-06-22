#ifndef MODBUS_H
#define MODBUS_H

#include "modbus_types.h"
#include <stddef.h>
#include <stdint.h>

// Outputs response/exception length
// If a context function is not implemented, the corresponding instruction will be considered
// not implemented.
// If return is 0, it indicates capacity was not sufficient
size_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                           uint8_t request_length, uint8_t *wire_response,
                           uint8_t response_capacity);

#endif // MODBUS_H
