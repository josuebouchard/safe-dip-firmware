#include "modbus_helpers.h"
#include "modbus_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))

static bool _pack_bools_to_u8(const bool *src, uint16_t quantity, uint8_t *dest)
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
bool modbus_pack_bools_to_u8_array(const bool *src, uint16_t bools_quantity, uint8_t *dest,
                                   uint8_t byte_count)
{
    // Assert - sanity check
    if (byte_count != modbus_packed_bits_to_bytes_count(bools_quantity))
    {
        return false;
    }

    // Process data
    bool success;
    for (uint8_t i = 0; i < byte_count; i++)
    {
        uint8_t chunk = MIN(8, bools_quantity);
        success = _pack_bools_to_u8(src, chunk, dest + i);
        src += chunk;
        bools_quantity -= 8; // This might underflow, but at that point it's unused
        if (!success)
            return false;
    }

    return true;
}

static bool _unpack_bools_from_u8(const uint8_t src, uint16_t quantity, bool *dest)
{
    if (quantity > 8)
    {
        return false;
    }

    for (uint8_t i = 0; i < quantity; i++)
    {
        dest[i] = (src >> i) & 1;
    }
    return true;
}

bool modbus_unpack_bools_from_u8_array(const uint8_t *src, uint16_t bools_quantity, bool *dest,
                                       uint8_t byte_count)
{
    // Assert - sanity check
    if (byte_count != modbus_packed_bits_to_bytes_count(bools_quantity))
    {
        return false;
    }

    // Process data
    bool success;
    for (uint8_t i = 0; i < byte_count; i++)
    {
        uint8_t chunk = MIN(8, bools_quantity);
        success = _unpack_bools_from_u8(src[i], chunk, dest);
        dest += chunk;
        bools_quantity -= 8; // This might underflow, but at that point it's unused
        if (!success)
            return false;
    }

    return true;
}

modbus_dispatch_result_t generate_exception_response(uint8_t function_code, uint8_t exception_code,
                                                     uint8_t *wire_response,
                                                     size_t response_capacity)
{
    // Set MSB to 1
    uint8_t exception_function_code = function_code | mask_u8(7);

    if (response_capacity < 2)
    {
        return (modbus_dispatch_result_t){.response_length = 0,
                                          .status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL};
    }

    // Response
    wire_response[0] = exception_function_code;
    wire_response[1] = exception_code;

    return (modbus_dispatch_result_t){.response_length = 2, .status = MODBUS_DISPATCH_ST_OK};
}