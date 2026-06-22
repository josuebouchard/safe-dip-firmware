#include "modbus.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIN(x, y) ((x) <= (y) ? x : y)

static inline uint8_t mask_u8(uint8_t shift) { return 1 << shift; }

static inline uint16_t modbus_read_u16_be(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static inline void modbus_write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8); // High part first
    bytes[1] = value & 0x00FF;        // Low part second
}

// Calculates `ceil(n/8)`
static inline uint8_t modbus_byte_count(uint16_t n)
{
    // 8 = 2^3 = 1 << 3;
    // 1/8 = 1 >> 3
    // The plus seven is done to round up
    return (n + (8 - 1)) >> 3;
}

static bool modbus_pack_bools_to_u8(const bool *src, uint16_t quantity, uint8_t *dest)
{
    // Assert - sanity check
    if (quantity > 8)
    {
        return false;
    }

    *dest = 0;
    for (uint8_t i = 0; i < quantity; i++)
    {
        *dest |= (src[i] == true ? 1 : 0) << i;
    }
    return true;
}

// It's assumed that the capacity of `dest` is at least as big as `byte_count`
static bool modbus_pack_bools_to_u8_array(const bool *src, uint16_t bools_quantity, uint8_t *dest,
                                          uint8_t byte_count)
{
    // Assert - sanity check
    if (byte_count != modbus_byte_count(bools_quantity))
    {
        return false;
    }

    // Process data
    bool success;
    for (uint8_t i = 0; i < byte_count; i++)
    {
        uint8_t chunk = MIN(8, bools_quantity);
        success = modbus_pack_bools_to_u8(src, chunk, dest + i);
        src += chunk;
        bools_quantity -= 8; // This might underflow, but at that point it's unused
        if (!success)
            return false;
    }

    return true;
}

static size_t generate_exception_response(uint8_t function_code, uint8_t exception_code,
                                          uint8_t *wire_response, uint8_t response_capacity)
{
    // Set MSB to 1
    uint8_t exception_function_code = function_code | mask_u8(7);

    if (response_capacity < 2)
    {
        // @todo: notify caller that it's not possible to write response
        return 0;
    }

    wire_response[0] = exception_function_code;
    wire_response[1] = exception_code;

    return 2;
}

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
