#include "modbus_helpers.h"
#include "modbus_holding_registers.h"

#include <stdint.h>

static const uint8_t READ_HOLDING_REGISTERS_FIXED_REQUEST_SIZE = 5;
static const uint8_t READ_HOLDING_REGISTERS_FIXED_RESPONSE_SIZE = 2;

// Function code: 0x03
modbus_dispatch_result_t dispatch_read_holding_registers(modbus_context_t *context,
                                                         const uint8_t *wire_request,
                                                         uint8_t request_length,
                                                         uint8_t *wire_response,
                                                         uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];

    // Validation - query function
    if (context->read_holding_registers == NULL)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }

    // Validation - request size
    if (request_length != READ_HOLDING_REGISTERS_FIXED_REQUEST_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    // Parse request
    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_registers = modbus_read_u16_be(wire_request + 3);

    // The Modbus limit for FC 0x03 is 1 to 125 registers.
    if (!(1 <= quantity_of_registers && quantity_of_registers <= 125))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    // FC (1), byte count (1), register values (2 bytes each)
    uint8_t byte_count = (uint8_t)(2U * quantity_of_registers);
    uint8_t total_response_size =
        (uint8_t)(READ_HOLDING_REGISTERS_FIXED_RESPONSE_SIZE + byte_count);

    // Validation - response capacity
    if (response_capacity < total_response_size)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL};
    }

    // Fetch register values from the application.
    uint16_t register_values[quantity_of_registers];
    modbus_query_state_t query_state =
        context->read_holding_registers(starting_address, register_values, quantity_of_registers);

    switch (query_state)
    {
    case MODBUS_QUERY_ST_BAD_ADDRESS:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_FAILURE:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    case MODBUS_QUERY_ST_OK:
        break;
    default:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    }

    // Build response.
    wire_response[0] = function_code;
    wire_response[1] = byte_count;

    for (uint16_t i = 0; i < quantity_of_registers; i++)
    {
        modbus_write_u16_be(wire_response + 2U + (2U * i), register_values[i]);
    }

    return (modbus_dispatch_result_t){.response_length = total_response_size,
                                      .status = MODBUS_DISPATCH_ST_OK};
}

static const uint8_t WRITE_SINGLE_REGISTER_FIXED_REQUEST_SIZE = 5;
static const uint8_t WRITE_SINGLE_REGISTER_FIXED_RESPONSE_SIZE = 5;

// Function code: 0x06
modbus_dispatch_result_t dispatch_write_holding_registers(modbus_context_t *context,
                                                          const uint8_t *wire_request,
                                                          uint8_t request_length,
                                                          uint8_t *wire_response,
                                                          uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];

    // Validation - query function
    if (context->write_holding_registers == NULL)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }

    // FC (1), register address (2), register value (2)
    if (request_length != WRITE_SINGLE_REGISTER_FIXED_REQUEST_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    uint16_t register_address = modbus_read_u16_be(wire_request + 1);
    uint16_t register_value = modbus_read_u16_be(wire_request + 3);

    if (response_capacity < WRITE_SINGLE_REGISTER_FIXED_RESPONSE_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL};
    }

    modbus_query_state_t query_state =
        context->write_holding_registers(register_address, &register_value, 1);

    switch (query_state)
    {
    case MODBUS_QUERY_ST_BAD_ADDRESS:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_FAILURE:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    case MODBUS_QUERY_ST_OK:
        break;
    default:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    }

    // A successful FC 0x06 response echoes the request.
    wire_response[0] = function_code;
    modbus_write_u16_be(wire_response + 1, register_address);
    modbus_write_u16_be(wire_response + 3, register_value);

    return (modbus_dispatch_result_t){.response_length = WRITE_SINGLE_REGISTER_FIXED_RESPONSE_SIZE,
                                      .status = MODBUS_DISPATCH_ST_OK};
}

static const uint8_t WRITE_MULTIPLE_REGISTERS_FIXED_REQUEST_SIZE = 6;
static const uint8_t WRITE_MULTIPLE_REGISTERS_FIXED_RESPONSE_SIZE = 5;

// Function code: 0x10
modbus_dispatch_result_t dispatch_write_multiple_holding_registers(modbus_context_t *context,
                                                                   const uint8_t *wire_request,
                                                                   uint8_t request_length,
                                                                   uint8_t *wire_response,
                                                                   uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];

    // Validation - query function
    if (context->write_holding_registers == NULL)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION, wire_response,
                                           response_capacity);
    }

    // FC (1), starting address (2), quantity (2), byte count (1)
    if (request_length < WRITE_MULTIPLE_REGISTERS_FIXED_REQUEST_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_registers = modbus_read_u16_be(wire_request + 3);
    uint8_t byte_count = wire_request[5];

    // The Modbus limit for FC 0x10 is 1 to 123 registers.
    if (!(1 <= quantity_of_registers && quantity_of_registers <= 123))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    uint8_t expected_byte_count = (uint8_t)(2U * quantity_of_registers);
    if (byte_count != expected_byte_count)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    // Reject truncated requests and requests with trailing bytes.
    if (request_length != (uint8_t)(WRITE_MULTIPLE_REGISTERS_FIXED_REQUEST_SIZE + byte_count))
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    if (response_capacity < WRITE_MULTIPLE_REGISTERS_FIXED_RESPONSE_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL};
    }

    // Decode register values only after the quantity and request length are validated.
    uint16_t register_values[quantity_of_registers];
    for (uint16_t i = 0; i < quantity_of_registers; i++)
    {
        register_values[i] = modbus_read_u16_be(wire_request + 6U + (2U * i));
    }

    modbus_query_state_t query_state =
        context->write_holding_registers(starting_address, register_values, quantity_of_registers);

    switch (query_state)
    {
    case MODBUS_QUERY_ST_BAD_ADDRESS:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_FAILURE:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    case MODBUS_QUERY_ST_OK:
        break;
    default:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE, wire_response,
                                           response_capacity);
    }

    // A successful FC 0x10 response contains the starting address and quantity written.
    wire_response[0] = function_code;
    modbus_write_u16_be(wire_response + 1, starting_address);
    modbus_write_u16_be(wire_response + 3, quantity_of_registers);

    return (modbus_dispatch_result_t){
        .response_length = WRITE_MULTIPLE_REGISTERS_FIXED_RESPONSE_SIZE,
        .status = MODBUS_DISPATCH_ST_OK,
    };
}
