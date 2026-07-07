#include "modbus_discrete_inputs.h"
#include "modbus_helpers.h"
#include "modbus_types.h"

static const uint8_t READ_DISCRETE_INPUTS_FIXED_REQUEST_SIZE = 5;
static const uint8_t READ_DISCRETE_INPUTS_FIXED_RESPONSE_SIZE = 2;

// Function code: 0x02
modbus_dispatch_result_t dispatch_read_discrete_inputs(modbus_context_t *context,
                                                       const uint8_t *wire_request,
                                                       uint8_t request_length,
                                                       uint8_t *wire_response,
                                                       uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];

    // Validations - query function
    if (context->read_discrete_inputs == NULL)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }

    // Validate wire request length before reading
    if (request_length != READ_DISCRETE_INPUTS_FIXED_REQUEST_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    // Parse rest of the request
    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_inputs = modbus_read_u16_be(wire_request + 3);
    uint8_t byte_count = modbus_packed_bits_to_bytes_count(quantity_of_inputs);

    // Validations - quantity of inputs
    if (!(1 <= quantity_of_inputs && quantity_of_inputs <= 2000))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    // Validations - response sufficient capacity
    uint8_t total_response_size = READ_DISCRETE_INPUTS_FIXED_RESPONSE_SIZE +
                                  byte_count; // FC (1), Byte Count (1), Input Status (byte_count)
    if (response_capacity < total_response_size)
    {
        return (modbus_dispatch_result_t){
            .response_length = 0,
            .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL,
        };
    }

    // Fetching
    bool discrete_inputs_values[quantity_of_inputs];
    modbus_query_state_t query_success =
        context->read_discrete_inputs(starting_address, discrete_inputs_values, quantity_of_inputs);

    // Validation - discrete inputs query
    switch (query_success)
    {
    case MODBUS_QUERY_ST_BAD_ADDRESS:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_FAILURE:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    case MODBUS_QUERY_ST_OK:
        break;
    }

    // Final response
    wire_response[0] = function_code;
    wire_response[1] = byte_count;
    bool packing_success = modbus_pack_bools_to_u8_array(discrete_inputs_values, quantity_of_inputs,
                                                         wire_response + 2, byte_count);
    if (!packing_success)
    {
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    }

    return (modbus_dispatch_result_t){.response_length = total_response_size,
                                      .status = MODBUS_DISPATCH_ST_OK};
}