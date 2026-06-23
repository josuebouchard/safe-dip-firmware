#ifndef MODBUS_HELPERS_H
#define MODBUS_HELPERS_H

#include "modbus_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline uint8_t mask_u8(uint8_t shift) { return 1 << shift; }
static inline uint16_t mask_u16(uint8_t shift) { return 1UL << shift; }

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

bool modbus_pack_bools_to_u8_array(const bool *src, uint16_t bools_quantity, uint8_t *dest,
                                   uint8_t byte_count);

bool modbus_unpack_bools_from_u8_array(const uint8_t *src, uint16_t bools_quantity, bool *dest,
                                       uint8_t byte_count);

modbus_dispatch_result_t generate_exception_response(uint8_t function_code, uint8_t exception_code,
                                                     uint8_t *wire_response,
                                                     uint8_t response_capacity);

#endif // MODBUS_HELPERS_H
