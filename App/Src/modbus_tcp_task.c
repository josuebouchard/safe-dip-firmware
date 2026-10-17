#include "modbus_tcp_task.h"
#include "ethernet_if.h" /* wiznet_close(), getSn_SR, getSn_RX_RSR, etc.    */
#include "socket.h"      /* socket(), listen(), recv(), send(), disconnect() */
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Non-blocking: issues DISCON and returns; the socket reaches SOCK_CLOSED
   on its own (FIN handshake or W5500 timeout) and run() reopens it. */
static void abort_connection(modbus_tcp_task_t *task)
{
    disconnect(task->socket_id);
    modbus_tcp_fsm_reset(&task->fsm);
    task->pending_write_size = 0;
}

/* Hands the pending response to the W5500. SOCK_BUSY means the previous
   send isn't ACKed yet or the TX buffer is full - keep it and retry next pass. */
static modbus_task_status_t send_pending(modbus_tcp_task_t *task)
{
    int32_t sent = send(task->socket_id, task->res_buf, task->pending_write_size);

    if (sent == SOCK_BUSY)
        return MODBUS_TASK_BUSY;

    if (sent != task->pending_write_size)
    {
        abort_connection(task);
        return MODBUS_TASK_ERROR;
    }

    task->pending_write_size = 0;
    return MODBUS_TASK_BUSY;
}

/* ------------------------------------------------------------------ */
/*  Init                                                               */
/* ------------------------------------------------------------------ */
void modbus_tcp_task_init(modbus_tcp_task_t *task, uint8_t socket_id, uint16_t port,
                          modbus_context_t *ctx)
{
    task->socket_id = socket_id;
    task->port = port;
    task->context = ctx;
    task->last_socket_state = 0xFF; /* force first-run print */
    task->pending_write_size = 0;

    /* Initialize and reset the Modbus FSM */
    task->fsm = (modbus_tcp_fsm_t){.context = ctx};
    modbus_tcp_fsm_reset(&task->fsm);
}

/* ------------------------------------------------------------------ */
/*  One cooperative step                                               */
/* ------------------------------------------------------------------ */
modbus_task_status_t modbus_tcp_task_run(modbus_tcp_task_t *task)
{
    const uint8_t sock = task->socket_id;

    uint8_t current_state = getSn_SR(sock);

    /* --- debug trace on state change --- */
    if (current_state != task->last_socket_state)
    {
        uint8_t eth_up = getPHYCFGR() & PHYCFGR_LNK_ON;
        uint8_t sn_ir = getSn_IR(sock);
        printf("Eth:%s | S:0x%02X->0x%02X | IR:%02X\r\n", eth_up ? "Y" : "N",
               task->last_socket_state, current_state, sn_ir);
        task->last_socket_state = current_state;
    }

    /* --- socket lifecycle state machine --- */
    switch (current_state)
    {

    case SOCK_CLOSED:
    {
        /* Non-blocking: send()/disconnect() return SOCK_BUSY instead of
           spinning on W5500 registers and stalling the superloop. */
        if (socket(sock, Sn_MR_TCP, task->port, SF_IO_NONBLOCK) != sock)
        {
            printf("task: socket() failed\r\n");
            return MODBUS_TASK_ERROR;
        }
        return MODBUS_TASK_IDLE;
    }

    case SOCK_INIT:
    {
        /* Keepalive every 10 s (5 s units), so a vanished peer frees the socket */
        setSn_KPALVTR(sock, 2);

        int8_t rc = listen(sock);
        if (rc != SOCK_OK)
        {
            printf("task: listen() err 0x%02X\r\n", rc);
            return MODBUS_TASK_ERROR;
        }
        return MODBUS_TASK_IDLE;
    }

    case SOCK_LISTEN:
        /* waiting for an incoming connection */
        return MODBUS_TASK_IDLE;

    case SOCK_ESTABLISHED:
    {
        /* Finish the previous response before reading the next request */
        if (task->pending_write_size > 0)
            return send_pending(task);

        uint16_t rx_avail = getSn_RX_RSR(sock);
        uint8_t read_size = modbus_tcp_fsm_read_length(&task->fsm);

        if (rx_avail < read_size)
            return MODBUS_TASK_IDLE; /* not enough bytes yet */

        /* --- recv --- */
        int32_t got = recv(sock, task->req_buf, read_size);
        if (got != read_size)
        {
            /* W5500 hardware fault — abort connection */
            abort_connection(task);
            return MODBUS_TASK_ERROR;
        }

        /* --- dispatch through the Modbus FSM --- */
        modbus_tcp_dispatch_result_t dispatch = modbus_tcp_fsm_dispatch(
            &task->fsm, task->req_buf, task->res_buf,
            sizeof(task->res_buf)); /* fixed: was 1024 on a 255-byte buffer */

        if (dispatch.status != MODBUS_TCP_DISPATCH_ST_OK)
        {
            modbus_tcp_fsm_reset(&task->fsm);
            return MODBUS_TASK_ERROR;
        }

        /* --- send response --- */
        task->pending_write_size = dispatch.write_size;
        if (task->pending_write_size > 0)
            return send_pending(task);

        return MODBUS_TASK_BUSY;
    }

    case SOCK_CLOSE_WAIT:
        /* Peer closed. Graceful close only - a wiznet_close() here would cut
           the FIN we just queued, now that disconnect() no longer waits. */
        abort_connection(task); /* clean slate for next client */
        return MODBUS_TASK_IDLE;

    default:
        return MODBUS_TASK_IDLE;
    }
}