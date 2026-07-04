#include "modbus.h"
#include "modbus_helpers.h"

#include "modbus_coils.h"
#include "modbus_discrete_inputs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

modbus_dispatch_result_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                                             uint8_t request_length, uint8_t *wire_response,
                                             size_t response_capacity)
{
    // Validate context has been set
    if (context == NULL)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_NO_CONTEXT};
    }

    // Validate wire request length before reading
    if (request_length < 1)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    uint8_t function_code = wire_request[0];
    switch (function_code)
    {
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        return dispatch_read_discrete_inputs(context, wire_request, request_length, wire_response,
                                             response_capacity);
    case MODBUS_FC_READ_COILS:
        return dispatch_read_coils(context, wire_request, request_length, wire_response,
                                   response_capacity);
    case MODBUS_FC_WRITE_SINGLE_COIL:
        return dispatch_write_single_coil(context, wire_request, request_length, wire_response,
                                          response_capacity);
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
        return dispatch_write_multiple_coils(context, wire_request, request_length, wire_response,
                                             response_capacity);
    default:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }
}
