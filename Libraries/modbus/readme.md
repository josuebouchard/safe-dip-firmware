# Modbus
 
A lightweight, portable Modbus RTU/PDU server library written in C99, designed for embedded systems. It has no heap allocation, no platform dependencies, and exposes a single dispatch function that maps incoming PDUs to user-provided callbacks.
 
## Features
 
- Clean separation between protocol logic and hardware — bring your own read/write functions
- No dynamic memory allocation — safe for bare-metal and RTOS environments
- Single public header (`modbus.h`) — internal implementation is fully encapsulated
- CMake-native, integrates as a static library

## Currently Supported Function Codes
 
| Code   | Name                 |
|--------|----------------------|
| `0x02` | Read Discrete Inputs |
| `0x01` | Read Coils           |
| `0x05` | Write Single Coil    |
| `0x0F` | Write Multiple Coils |

Holding registers and input registers are planned but not yet implemented.

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
size_t response_len = modbus_dispatch_PDU(
    &ctx,
    request_buf,      // raw bytes from the wire (PDU only, no address/CRC)
    request_len,
    response,
    sizeof(response)
);
 
if (response_len > 0) {
    // transmit response_len bytes from response[]
}
```
 
## API Reference
 
### `modbus_dispatch_PDU`
 
```c
size_t modbus_dispatch_PDU(
    modbus_context_t *context,
    const uint8_t    *wire_request,
    uint8_t           request_length,
    uint8_t          *wire_response,
    uint8_t           response_capacity
);
```
 
Dispatches a Modbus PDU request and writes the response into `wire_response`.
 
Returns the number of bytes written into `wire_response`, including exception responses. Returns `0` if `response_capacity` was insufficient to write any response.
 
> **Note:** This function operates on the PDU layer only. Framing (device address, CRC) is the responsibility of the caller.

### `modbus_context_t`
 
All fields except `user_context` are function pointers. Unimplemented function codes will return an `ILLEGAL FUNCTION` exception response. Set unused fields to `NULL`.
 
| Field | Signature | Description |
|-------|-----------|-------------|
| `read_discrete_inputs` | `(uint16_t addr, bool *dest, uint16_t qty, void *user_context)` | FC 0x02 |
| `read_coils` | `(uint16_t addr, bool *dest, uint16_t qty, void *user_context)` | FC 0x01 |
| `write_coils` | `(uint16_t addr, const bool *src, uint16_t qty, void *user_context)` | FC 0x05, 0x0F |
| `read_input_registers` | `(uint16_t addr, uint16_t *dest, uint16_t qty, void *user_context)` | FC 0x04 (not yet dispatched) |
| `read_holding_registers` | `(uint16_t addr, uint16_t *dest, uint16_t qty, void *user_context)` | FC 0x03 (not yet dispatched) |
| `write_holding_registers` | `(uint16_t addr, const uint16_t *src, uint16_t qty, void *user_context)` | FC 0x06, 0x10 (not yet dispatched) |
| `user_context` | `void *` | Passed back as the last argument of every callback. May be `NULL`. |
 
### `modbus_query_state_t`
 
Returned by all callback functions:
 
| Value | Meaning |
|-------|---------|
| `MODBUS_QUERY_ST_OK` | Success |
| `MODBUS_QUERY_ST_BAD_ADDRESS` | Address out of range → `ILLEGAL DATA ADDRESS` exception |
| `MODBUS_QUERY_ST_FAILURE` | Internal error → `SERVER FAILURE` exception |
 
## Response Buffer Sizing
 
The Modbus spec limits PDUs to 253 bytes. Providing a 253-byte response buffer guarantees `modbus_dispatch_PDU` will never return `0` due to capacity:
 
```c
uint8_t response[253];
```
 
## Project Structure
 
```
modbus/
├── Inc/
│   ├── Public/          # Consumer-facing headers (modbus.h, modbus_types.h)
│   └── Private/         # Internal headers (not exposed to library consumers)
└── Src/                 # Implementation files
```
