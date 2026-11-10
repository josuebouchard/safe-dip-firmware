#include "modbus_input_registers.h"
#include "modbus_helpers.h"

#include <stdint.h>

static const uint8_t READ_INPUT_REGISTERS_FIXED_REQUEST_SIZE = 5;
static const uint8_t READ_INPUT_REGISTERS_FIXED_RESPONSE_SIZE = 2;

// Function code: 0x04
modbus_dispatch_result_t dispatch_read_input_registers(modbus_context_t *context,
                                                       const uint8_t *wire_request,
                                                       uint8_t request_length,
                                                       uint8_t *wire_response,
                                                       size_t response_capacity)
{
    uint8_t function_code = wire_request[0];

    // Validation - query function
    if (context->read_input_registers == NULL)
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_FUNCTION,
                                           wire_response, response_capacity);
    }

    // FC (1), starting address (2), quantity of input registers (2)
    if (request_length != READ_INPUT_REGISTERS_FIXED_REQUEST_SIZE)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_MALFORMED_PDU};
    }

    uint16_t starting_address = modbus_read_u16_be(wire_request + 1);
    uint16_t quantity_of_registers = modbus_read_u16_be(wire_request + 3);

    // The Modbus limit for FC 0x04 is 1 to 125 registers.
    if (!(1 <= quantity_of_registers && quantity_of_registers <= 125))
    {
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                           wire_response, response_capacity);
    }

    // FC (1), byte count (1), register values (2 bytes each)
    uint8_t byte_count = (uint8_t)(2U * quantity_of_registers);
    uint8_t total_response_size =
        (uint8_t)(READ_INPUT_REGISTERS_FIXED_RESPONSE_SIZE + byte_count);

    if (response_capacity < total_response_size)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL};
    }

    uint16_t register_values[quantity_of_registers];
    modbus_query_state_t query_state = context->read_input_registers(
        starting_address, register_values, quantity_of_registers, context->user_context);

    switch (query_state)
    {
    case MODBUS_QUERY_ST_BAD_ADDRESS:
        return generate_exception_response(function_code, MODBUS_EX_ILLEGAL_DATA_ADDRESS,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_FAILURE:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE,
                                           wire_response, response_capacity);
    case MODBUS_QUERY_ST_OK:
        break;
    default:
        return generate_exception_response(function_code, MODBUS_EX_SERVER_FAILURE,
                                           wire_response, response_capacity);
    }

    wire_response[0] = function_code;
    wire_response[1] = byte_count;

    for (uint16_t i = 0; i < quantity_of_registers; i++)
    {
        modbus_write_u16_be(wire_response + 2U + (2U * i), register_values[i]);
    }

    return (modbus_dispatch_result_t){.response_length = total_response_size,
                                      .status = MODBUS_DISPATCH_ST_OK};
}
