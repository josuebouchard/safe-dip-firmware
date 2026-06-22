#include "modbus.h"
#include "modbus_helpers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static size_t dispatch_read_discrete_inputs(modbus_context_t *context, const uint8_t *wire_request,
                                            uint8_t request_length, uint8_t *wire_response,
                                            uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];
    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_inputs = modbus_read_u16_be(wire_request + 3);

    uint8_t byte_count = modbus_byte_count(quantity_of_inputs);
    uint8_t total_size = 2 + byte_count; // FC (1), Byte Count (1), Input Status (byte_count)

    // Validations - quantity of inputs
    if (!(1 <= quantity_of_inputs && quantity_of_inputs <= 2000))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }
    // Validations - sufficient capacity
    if (response_capacity < total_size)
    {
        //@todo: throw error
        return 0;
    }

    // Fetching
    bool discrete_inputs_values[quantity_of_inputs];
    modbus_query_state_t query_success =
        context->read_discrete_inputs(starting_address, discrete_inputs_values, quantity_of_inputs);

    // Validation - discrete inputs query
    if (query_success == MODBUS_QUERY_ST_BAD_ADDRESS)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    }
    else if (query_success == MODBUS_QUERY_ST_FAILURE)
    {
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    }

    // Final response
    wire_response[0] = function_code;
    wire_response[1] = byte_count;
    bool packing_success = modbus_pack_bools_to_u8_array(discrete_inputs_values, quantity_of_inputs,
                                                         wire_response + 2, byte_count);
    if (!packing_success)
    {
        return 0;
    }

    return total_size;
}

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
