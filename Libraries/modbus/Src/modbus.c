#include "modbus.h"
#include "modbus_helpers.h"

#include "modbus_discrete_inputs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                           uint8_t request_length, uint8_t *wire_response,
                           uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];
    switch (function_code)
    {
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        return dispatch_read_discrete_inputs(context, wire_request, request_length, wire_response,
                                             response_capacity);
    default:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }
}
