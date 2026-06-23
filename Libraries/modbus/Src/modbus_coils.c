#include "modbus_coils.h"
#include "modbus_helpers.h"
#include "modbus_types.h"
#include <stdint.h>

// Function Code: 0x01
size_t dispatch_read_coils(modbus_context_t *context, const uint8_t *wire_request,
                           uint8_t request_length, uint8_t *wire_response,
                           uint8_t response_capacity)
{
    // @todo: validate request_length == 5

    // Parsing request
    uint8_t function_code = wire_request[0];
    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_coils = modbus_read_u16_be(wire_request + 3);

    // Response calculations
    uint8_t byte_count = modbus_byte_count(quantity_of_coils);
    uint8_t total_response_size =
        2 + byte_count; // FC (1), Byte Count (1), Coil Status (byte_count)

    // Validations - quantity of inputs
    if (!(1 <= quantity_of_coils && quantity_of_coils <= 2000))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }
    // Validations - sufficient response capacity
    if (response_capacity < total_response_size)
    {
        //@todo: throw error
        return 0;
    }

    // Fetching
    bool coils_values[quantity_of_coils];
    modbus_query_state_t query_success =
        context->read_coils(starting_address, coils_values, quantity_of_coils);

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
    bool packing_success = modbus_pack_bools_to_u8_array(coils_values, quantity_of_coils,
                                                         wire_response + 2, byte_count);
    if (!packing_success)
    {
        return 0;
    }

    return total_response_size;
}

// Function Code: 0x05
size_t dispatch_write_single_coil(modbus_context_t *context, const uint8_t *wire_request,
                                  uint8_t request_length, uint8_t *wire_response,
                                  uint8_t response_capacity)
{
    // @todo: validate request_length == 5

    // Parsing request
    uint8_t function_code = wire_request[0];
    uint16_t output_address = modbus_read_u16_be(wire_request + 1);
    uint16_t numeric_output_value = modbus_read_u16_be(wire_request + 3);
    bool output_value;

    // There are only two valid values
    switch (numeric_output_value)
    {
    case 0x0000:
        output_value = false;
        break;

    case 0xFF00:
        output_value = true;
        break;

    default:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    // Response calculations
    const uint8_t total_response_size = 5; // FC (1), Output Address (2), Output Value (2)

    // Validations - sufficient response capacity
    if (response_capacity < total_response_size)
    {
        return 0;
    }

    modbus_query_state_t query_success = context->write_coils(output_address, &output_value, 1);
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
    modbus_write_u16_be(wire_response + 1, output_address);
    modbus_write_u16_be(wire_response + 3, numeric_output_value);
    return total_response_size;
}

// Function code: 0x0F
size_t dispatch_write_multiple_coils(modbus_context_t *context, const uint8_t *wire_request,
                                     uint8_t request_length, uint8_t *wire_response,
                                     uint8_t response_capacity)
{
    // @todo: validate request_length >= 6

    // Parsing request
    uint8_t function_code = wire_request[0];
    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_outputs = modbus_read_u16_be(wire_request + 3);
    uint8_t byte_count = wire_request[5];

    const uint8_t total_response_size = 5;

    // @todo: validate request_length = 6 + byte_count

    // Validations - sufficient response capacity
    if (response_capacity < total_response_size)
    {
        //@todo: throw error
        return 0;
    }
    // Validations - quantity of inputs
    if (!(1 <= quantity_of_outputs && quantity_of_outputs <= 0x07B0))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }
    // Validations - byte_count well calculated
    if (byte_count != modbus_byte_count(quantity_of_outputs))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    bool outputs[quantity_of_outputs];
    if (!modbus_unpack_bools_from_u8_array(wire_request + 6, quantity_of_outputs, outputs,
                                           byte_count))
    {
        return 0;
    }

    modbus_query_state_t query_success =
        context->write_coils(starting_address, outputs, quantity_of_outputs);
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
    modbus_write_u16_be(wire_response + 1, starting_address);
    modbus_write_u16_be(wire_response + 3, quantity_of_outputs);
    return total_response_size;
}
