#include "modbus.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline uint16_t modbus_read_u16_be(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static inline bool is_between_u8(uint8_t value, uint8_t min, uint8_t max)
{
    return min <= value && value <= max;
}

static size_t dispatch_read_discrete_inputs(modbus_context_t *context,
                                                   const uint8_t *wire_request,
                                                   uint8_t request_length, uint8_t *wire_response,
                                                   uint8_t response_capacity)
{
    
}

size_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                           uint8_t request_length, uint8_t *wire_response,
                           uint8_t response_capacity)
{
    uint8_t function_code = wire_request[0];
    switch (function_code)
    {
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        return dispatch_read_discrete_inputs(context, wire_request, request_length,
                                                    wire_response, response_capacity);
    }
}
