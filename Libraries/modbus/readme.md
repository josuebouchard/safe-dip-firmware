# Modbus
 
A lightweight, portable Modbus server library written in C99, designed for embedded systems. It has no heap allocation and no platform dependencies. Its core is a single dispatch function that maps an incoming PDU to user-provided callbacks, and a small Modbus TCP module adds the MBAP framing on top of it.
 
## Features
 
- Clean separation between protocol logic and hardware — bring your own read/write functions
- No dynamic memory allocation — safe for bare-metal and RTOS environments
- One dispatch function (`modbus.h`) for the PDU, and an optional TCP framing module (`modbus_tcp.h`) — the internal implementation is fully encapsulated
- CMake-native, integrates as a static library

## Supported Function Codes

| Code   | Name                      |
|--------|---------------------------|
| `0x01` | Read Coils                |
| `0x02` | Read Discrete Inputs      |
| `0x03` | Read Holding Registers    |
| `0x04` | Read Input Registers      |
| `0x05` | Write Single Coil         |
| `0x06` | Write Single Register     |
| `0x0F` | Write Multiple Coils      |
| `0x10` | Write Multiple Registers  |

Any other function code returns an `ILLEGAL FUNCTION` exception response.

## Getting Started
 
### 1. Add to your CMake project
 
```cmake
add_subdirectory(modbus)
target_link_libraries(your_target PRIVATE modbus)
```
 
### 2. Implement your callbacks
 
The library does not access hardware directly. You provide function pointers that map Modbus addresses to your device's memory or peripherals:
 
```c
#include "modbus.h"
 
static bool coils[100];
static bool discrete_inputs[100];
 
modbus_query_state_t my_read_coils(uint16_t start_addr, bool *dest, uint16_t quantity, void *user_context)
{
    if (start_addr + quantity > 100)
        return MODBUS_QUERY_ST_BAD_ADDRESS;
 
    for (uint16_t i = 0; i < quantity; i++)
        dest[i] = coils[start_addr + i];
 
    return MODBUS_QUERY_ST_OK;
}
 
modbus_query_state_t my_write_coils(uint16_t start_addr, const bool *src, uint16_t quantity, void *user_context)
{
    if (start_addr + quantity > 100)
        return MODBUS_QUERY_ST_BAD_ADDRESS;
 
    for (uint16_t i = 0; i < quantity; i++)
        coils[start_addr + i] = src[i];
 
    return MODBUS_QUERY_ST_OK;
}
 
modbus_query_state_t my_read_discrete_inputs(uint16_t start_addr, bool *dest, uint16_t quantity, void *user_context)
{
    if (start_addr + quantity > 100)
        return MODBUS_QUERY_ST_BAD_ADDRESS;
 
    for (uint16_t i = 0; i < quantity; i++)
        dest[i] = discrete_inputs[start_addr + i];
 
    return MODBUS_QUERY_ST_OK;
}
```
 
### 3. Create a context and dispatch
 
```c
modbus_context_t ctx = {
    .read_coils          = my_read_coils,
    .write_coils         = my_write_coils,
    .read_discrete_inputs = my_read_discrete_inputs,
};
 
// In your UART receive handler:
uint8_t response[253];
modbus_dispatch_result_t result = modbus_dispatch_PDU(
    &ctx,
    request_buf,      // raw bytes from the wire (PDU only, no address/CRC)
    request_len,
    response,
    sizeof(response)
);

if (result.status == MODBUS_DISPATCH_ST_OK) {
    // transmit result.response_length bytes from response[]
}
```
 
## API Reference
 
### `modbus_dispatch_PDU`

```c
modbus_dispatch_result_t modbus_dispatch_PDU(
    modbus_context_t *context,
    const uint8_t    *wire_request,
    uint8_t           request_length,
    uint8_t          *wire_response,
    size_t            response_capacity
);
```

Dispatches a Modbus PDU request and writes the response into `wire_response`.

The result holds a `status` and a `response_length`. With `MODBUS_DISPATCH_ST_OK`, `response_length` bytes of a valid response (an exception response included) are ready to transmit. Any other status means nothing was written:

| Status | Meaning |
|--------|---------|
| `MODBUS_DISPATCH_ST_OK` | A response was written |
| `MODBUS_DISPATCH_ST_NO_CONTEXT` | `context` is `NULL` |
| `MODBUS_DISPATCH_ST_MALFORMED_PDU` | The request is empty or too short for its function code |
| `MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL` | `response_capacity` cannot hold the response |
| `MODBUS_DISPATCH_ST_INTERNAL_ERROR` | An unexpected internal failure |

> **Note:** This function operates on the PDU layer only. Framing (device address, CRC, or the MBAP header of Modbus TCP) is the responsibility of the caller, or of `modbus_tcp.h` for TCP.

### `modbus_context_t`
 
All fields except `user_context` are function pointers. Unimplemented function codes will return an `ILLEGAL FUNCTION` exception response. Set unused fields to `NULL`.
 
| Field | Signature | Description |
|-------|-----------|-------------|
| `read_discrete_inputs` | `(uint16_t addr, bool *dest, uint16_t qty, void *user_context)` | FC 0x02 |
| `read_coils` | `(uint16_t addr, bool *dest, uint16_t qty, void *user_context)` | FC 0x01 |
| `write_coils` | `(uint16_t addr, const bool *src, uint16_t qty, void *user_context)` | FC 0x05, 0x0F |
| `read_input_registers` | `(uint16_t addr, uint16_t *dest, uint16_t qty, void *user_context)` | FC 0x04 |
| `read_holding_registers` | `(uint16_t addr, uint16_t *dest, uint16_t qty, void *user_context)` | FC 0x03 |
| `write_holding_registers` | `(uint16_t addr, const uint16_t *src, uint16_t qty, void *user_context)` | FC 0x06, 0x10 |
| `user_context` | `void *` | Passed back as the last argument of every callback. May be `NULL`. |
 
### `modbus_query_state_t`
 
Returned by all callback functions:
 
| Value | Meaning |
|-------|---------|
| `MODBUS_QUERY_ST_OK` | Success |
| `MODBUS_QUERY_ST_BAD_ADDRESS` | Address out of range → `ILLEGAL DATA ADDRESS` exception |
| `MODBUS_QUERY_ST_FAILURE` | Internal error → `SERVER FAILURE` exception |
 
## Response Buffer Sizing

The Modbus spec limits PDUs to 253 bytes. A 253-byte response buffer is enough for any response, so `modbus_dispatch_PDU` will never report `MODBUS_DISPATCH_ST_RESPONSE_TOO_SMALL` because of capacity:

```c
uint8_t response[253];
```

## Modbus TCP

`modbus_tcp.h` frames the PDU for Modbus TCP. A request arrives as a 7-byte MBAP header followed by the PDU, and a small state machine (`modbus_tcp_fsm_t`) takes them in two steps:

1. `modbus_tcp_fsm_read_length()` says how many bytes to read next: 7 for the header, then the PDU length that the header announced.
2. `modbus_tcp_fsm_dispatch()` takes those bytes. After the header it checks it (the protocol identifier must be 0 and the length must hold 1 to 253 bytes). After the PDU it calls `modbus_dispatch_PDU`, puts the MBAP header in front of the response, and returns the number of bytes to send in `write_size`.

The result also carries a `status` (`MODBUS_TCP_DISPATCH_ST_OK`, `..._BAD_MBAP`, `..._UNSUPPORTED_PROTOCOL_ID`, or `..._PDU_ERROR` with the reason in `pdu_status`). `modbus_tcp_fsm_reset()` starts the state machine over, for example after a connection is closed.

## Project Structure
 
```
modbus/
├── Inc/
│   ├── Public/          # Consumer-facing headers (modbus.h, modbus_types.h, modbus_tcp.h)
│   └── Private/         # Internal headers (not exposed to library consumers)
└── Src/                 # Implementation files
```
