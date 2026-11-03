#ifndef MODBUS_TCP_TASK_H
#define MODBUS_TCP_TASK_H

#include "modbus_tcp.h"
#include <stdbool.h>
#include <stdint.h>


/* ------------------------------------------------------------------ */
/*  Cooperative task return values                                      */
/* ------------------------------------------------------------------ */
typedef enum
{
    MODBUS_TASK_OK = 0, /* stepped successfully, nothing notable          */
    MODBUS_TASK_BUSY,   /* actively processing a request this tick        */
    MODBUS_TASK_IDLE,   /* waiting (closed, listening, or no data)        */
    MODBUS_TASK_ERROR,  /* a fatal socket error occurred; FSM was reset   */
} modbus_task_status_t;

/* ------------------------------------------------------------------ */
/*  Task handle                                                        */
/*                                                                     */
/*  Owns its own buffers and FSM state so it is safe to call           */
/*  modbus_tcp_task_run() every super-loop iteration.                 */
/*  NOT reentrant — one instance per socket.                           */
/* ------------------------------------------------------------------ */
#define MODBUS_TASK_REQ_BUF_SIZE 1024
#define MODBUS_TASK_RES_BUF_SIZE 1024

typedef struct
{
    /* configuration (set once at init) */
    uint8_t socket_id;
    uint16_t port;
    modbus_context_t *context;

    /* runtime state */
    uint8_t last_socket_state;
    modbus_tcp_fsm_t fsm;
    uint16_t pending_write_size; /* response in res_buf the W5500 hasn't taken yet */

    /* buffers (owned by the task so they persist across calls) */
    uint8_t req_buf[MODBUS_TASK_REQ_BUF_SIZE];
    uint8_t res_buf[MODBUS_TASK_RES_BUF_SIZE];

} modbus_tcp_task_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief  Initialize the task. Call once before the super-loop.
     * @note   Does NOT open the socket — that happens lazily on first run().
     */
    void modbus_tcp_task_init(modbus_tcp_task_t *task, uint8_t socket_id, uint16_t port,
                              modbus_context_t *ctx);

    /**
     * @brief  Advance the task by one non-blocking step.
     * @retval MODBUS_TASK_OK / BUSY / IDLE / ERROR
     * @note   Call every super-loop iteration. Never blocks or delays.
     *         Not reentrant — do not call concurrently on the same instance.
     */
    modbus_task_status_t modbus_tcp_task_run(modbus_tcp_task_t *task);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TCP_TASK_H */