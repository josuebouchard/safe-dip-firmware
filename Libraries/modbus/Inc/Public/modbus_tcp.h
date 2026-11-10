#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modbus_types.h"

#define MODBUS_TCP_MBAP_BYTES_SIZE 7

// The largest PDU the Modbus specification allows: function code plus 252 data bytes.
#define MODBUS_TCP_MAX_PDU_SIZE 253

typedef struct modbus_tcp_mbap
{
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t pdu_length;
    uint8_t unit_id;
} modbus_tcp_mbap_t;

// wire_request must be at least 7 bytes long. Fails on a protocol id other than 0, and
// on a length field that doesn't hold a PDU of 1 to MODBUS_TCP_MAX_PDU_SIZE bytes.
bool modbus_tcp_parse_mbap(const uint8_t *wire_request, uint8_t request_length,
                           modbus_tcp_mbap_t *out_mbap);

// Response is always 7 bytes long
void modbus_tcp_write_mbap(modbus_tcp_mbap_t *mbap, uint8_t *wire_response);

typedef enum modbus_tcp_fsm_state
{
    MODBUS_TCP_STATE_PARSE_MBAP,
    MODBUS_TCP_STATE_PARSE_PDU,
} modbus_tcp_fsm_state_t;

typedef struct modbus_tcp_fsm
{
    modbus_tcp_fsm_state_t state;
    modbus_context_t *context;

    // Extra data
    modbus_tcp_mbap_t mbap;
} modbus_tcp_fsm_t;

typedef enum modbus_tcp_fsm_status
{
    MODBUS_TCP_DISPATCH_ST_OK,
    MODBUS_TCP_DISPATCH_ST_BAD_MBAP,
    MODBUS_TCP_DISPATCH_ST_UNSUPPORTED_PROTOCOL_ID,
    MODBUS_TCP_DISPATCH_ST_PDU_ERROR, // ask pdu_status for the reason
} modbus_tcp_fsm_status_t;

typedef struct modbus_tcp_dispatch_result
{
    uint16_t write_size;
    modbus_tcp_fsm_status_t status;
    modbus_dispatch_status_t pdu_status; // only meaningful when status == PDU_ERROR

} modbus_tcp_dispatch_result_t;

uint8_t modbus_tcp_fsm_read_length(modbus_tcp_fsm_t *fsm);

void modbus_tcp_fsm_reset(modbus_tcp_fsm_t *fsm);

modbus_tcp_dispatch_result_t modbus_tcp_fsm_dispatch(modbus_tcp_fsm_t *fsm,
                                                     const uint8_t *wire_request,
                                                     uint8_t *wire_response,
                                                     size_t response_capacity);

#endif // MODBUS_TCP_H
