#include "modbus_tcp.h"
#include "modbus.h"
#include "modbus_helpers.h"
#include "modbus_types.h"

#include <stddef.h>
#include <stdint.h>

bool modbus_tcp_parse_mbap(const uint8_t *wire_request, uint8_t request_length,
                           modbus_tcp_mbap_t *out_mbap)
{
    if (request_length < 7)
    {
        return false; // Message not long enough
    }

    out_mbap->transaction_id = modbus_read_u16_be(wire_request + 0);
    out_mbap->protocol_id = modbus_read_u16_be(wire_request + 2);
    uint16_t length = modbus_read_u16_be(wire_request + 4);
    out_mbap->pdu_length = length - 1; // Subtract one for the unit_id
    out_mbap->unit_id = wire_request[6];

    // Sanity check
    // PDU length should at least be 1 (for the unit_id)
    // Protocol should be 0 for Modbus TCP
    if (length < 1 || out_mbap->protocol_id != 0)
    {
        return false;
    }

    return true;
}

void modbus_tcp_write_mbap(modbus_tcp_mbap_t *mbap, uint8_t *wire_response)
{
    modbus_write_u16_be(wire_response + 0, mbap->transaction_id);
    modbus_write_u16_be(wire_response + 2, mbap->protocol_id);
    modbus_write_u16_be(wire_response + 4, mbap->pdu_length + 1); // Add one for the unit_id
    wire_response[6] = mbap->unit_id;
}

uint8_t modbus_tcp_fsm_read_length(modbus_tcp_fsm_t *fsm)
{
    switch (fsm->state)
    {
    case MODBUS_TCP_STATE_PARSE_MBAP:
        return MODBUS_TCP_MBAP_BYTES_SIZE;
    case MODBUS_TCP_STATE_PARSE_PDU:
        return fsm->mbap.pdu_length;
    }
}

void modbus_tcp_fsm_reset(modbus_tcp_fsm_t *fsm)
{
    fsm->state = MODBUS_TCP_STATE_PARSE_MBAP;
    fsm->mbap = (modbus_tcp_mbap_t){0};
}

modbus_tcp_dispatch_result_t modbus_tcp_fsm_dispatch(modbus_tcp_fsm_t *fsm,
                                                     const uint8_t *wire_request,
                                                     uint8_t *wire_response,
                                                     size_t response_capacity)
{
    uint8_t read_length = modbus_tcp_fsm_read_length(fsm);
    switch (fsm->state)
    {
    case MODBUS_TCP_STATE_PARSE_MBAP:
    {
        // Parse MBAP
        bool success = modbus_tcp_parse_mbap(wire_request, read_length, &(fsm->mbap));
        if (!success)
        {
            return (modbus_tcp_dispatch_result_t){
                .write_size = 0,
                .status = MODBUS_TCP_DISPATCH_ST_BAD_MBAP,
            };
        }

        fsm->state = MODBUS_TCP_STATE_PARSE_PDU;
        return (modbus_tcp_dispatch_result_t){
            .write_size = 0,
            .status = MODBUS_TCP_DISPATCH_ST_OK,
        };
    }
    case MODBUS_TCP_STATE_PARSE_PDU:
    {
        if (response_capacity < MODBUS_TCP_MBAP_BYTES_SIZE)
        {
            return (modbus_tcp_dispatch_result_t){
                .write_size = 0,
                .status = MODBUS_TCP_DISPATCH_ST_PDU_ERROR,
                .pdu_status = MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL,
            };
        }

        // Dispatch instruction - leave room for MBAP header
        modbus_dispatch_result_t dispatch_state =
            modbus_dispatch_PDU(fsm->context, wire_request, fsm->mbap.pdu_length,
                                wire_response + MODBUS_TCP_MBAP_BYTES_SIZE,
                                response_capacity - MODBUS_TCP_MBAP_BYTES_SIZE);

        // @todo: can check errors of modbus_dispatch
        if (dispatch_state.status != MODBUS_DISPATCH_ST_OK)
        {
            return (modbus_tcp_dispatch_result_t){
                .write_size = 0,
                .status = MODBUS_TCP_DISPATCH_ST_PDU_ERROR,
                .pdu_status = dispatch_state.status,
            };
        }

        // Set mbap length
        fsm->mbap.pdu_length = dispatch_state.response_length;

        // Write mbap to response buffer
        modbus_tcp_write_mbap(&(fsm->mbap), wire_response);

        fsm->state = MODBUS_TCP_STATE_PARSE_MBAP;
        return (modbus_tcp_dispatch_result_t){
            .write_size = MODBUS_TCP_MBAP_BYTES_SIZE + dispatch_state.response_length,
            .status = MODBUS_TCP_DISPATCH_ST_OK,
        };
    }
    }
}
