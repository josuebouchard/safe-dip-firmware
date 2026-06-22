#ifndef MODBUS_H
#define MODBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// MODBUS Request PDU, mb_req_pdu
// MODBUS Response PDU, mb_rsp_pdu
// MODBUS Exception Response PDU, mb_excep_rsp_pdu

typedef enum modbus_function_code
{
    // Discrete inputs (read-only)
    MODBUS_FC_READ_DISCRETE_INPUTS = 0x02,

    // Coils (read + write)
    MODBUS_FC_READ_COILS = 0x01,
    MODBUS_FC_WRITE_SINGLE_COIL = 0x05,
    MODBUS_FC_WRITE_MULTIPLE_COILS = 0x0F,

    // Input registers (read-only)
    MODBUS_FC_READ_INPUT_REGISTERS = 0x04,

    // Holding registers (read + write)
    MODBUS_FC_READ_HOLDING_REGISTERS = 0x03,
    MODBUS_FC_WRITE_SINGLE_REGISTER = 0x06,
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS = 0x10,
} modbus_function_code_t;

typedef enum modbus_exception_code
{
    MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    MODBUS_EX_SERVER_FAILURE = 0x04,
} modbus_exception_code_t;

// Function defs

typedef bool (*modbus_read_coils_fn)(uint16_t start_addr, bool *dest, uint16_t quantity);

typedef bool (*modbus_write_coils_fn)(uint16_t start_addr, const bool *src, uint16_t quantity);

typedef bool (*modbus_read_registers_fn)(uint16_t start_addr, uint16_t *dest, uint16_t quantity);

typedef bool (*modbus_write_registers_fn)(uint16_t start_addr, const uint16_t *src,
                                          uint16_t quantity);

// Modbus context does not own any of its values
typedef struct modbus_context
{
    // Discrete inputs (read-only)
    modbus_read_coils_fn read_discrete_inputs;

    // Coils (read + write)
    modbus_read_coils_fn read_coils;
    modbus_write_coils_fn write_coils;

    // Input registers (read-only)
    modbus_read_registers_fn read_input_registers;

    // Holding registers (read + write)
    modbus_read_registers_fn read_holding_registers;
    modbus_write_registers_fn write_holding_registers;
} modbus_context_t;

// Outputs response/exception length
// If a context function is not implemented, the corresponding instruction will be considered
// not implemented.
size_t modbus_dispatch_PDU(modbus_context_t *context, const uint8_t *wire_request,
                           uint8_t request_length, uint8_t *wire_response,
                           uint8_t response_capacity);

#endif // MODBUS_H
