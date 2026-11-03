# Architecture and Design Decisions — Modbus Library

This document explains the reasoning behind the key architectural decisions made during the design and implementation of this library. It is intended for anyone who wants to understand not just what the library does, but why it is structured the way it is.

---

## The Core Design Goal: Separation of Protocol from Hardware

The most important decision made in this project was to completely decouple the Modbus protocol logic from any hardware or platform dependency.

In many embedded Modbus implementations, the protocol logic and hardware access are mixed together — the library reads directly from memory-mapped registers, calls HAL functions, or assumes a specific UART peripheral. This makes the library fast to write but impossible to reuse, test, or port.

This library takes the opposite approach. It knows nothing about hardware. It takes raw bytes in, writes raw bytes out, and delegates all data access to the caller through function pointers:

```c
typedef struct modbus_context
{
    modbus_read_coils_fn  read_coils;
    modbus_write_coils_fn write_coils;
    modbus_read_coils_fn  read_discrete_inputs;
    // ...
    void *user_context;   // Passed back to every callback
} modbus_context_t;
```

The caller provides implementations of these functions that map Modbus addresses to whatever the application needs — GPIO pins, memory arrays, sensor readings, or communication peripherals. The library never needs to change when the hardware does.

This has three concrete benefits:

**Portability.** The library compiles and runs on any C99 platform, including the host development machine. There is no STM32-specific code anywhere in the library itself.

**Testability.** Because there is no hardware dependency, unit tests run natively on the development machine using mock implementations of the function pointers. No hardware, no emulator, no cross-compilation required.

**Flexibility.** Multiple instances of `modbus_context_t` can coexist, each pointing to different data sources through its own `user_context`. A single application could serve different Modbus address spaces on different interfaces simultaneously.

---

## A Single Public Entry Point

The entire public API surface is one function:

```c
size_t modbus_dispatch_PDU(modbus_context_t *context,
                           const uint8_t    *wire_request,
                           uint8_t           request_length,
                           uint8_t          *wire_response,
                           uint8_t           response_capacity);
```

This is intentional. From the caller's perspective, Modbus is a request-response protocol: bytes arrive, bytes go out. The internal complexity — function code dispatch, validation, packing, exception generation — is an implementation detail the caller should not need to understand.

The function returns the number of bytes written into `wire_response`, including exception responses. A return value of `0` means the response buffer was too small to write anything. In all other cases, a valid Modbus PDU has been written and is ready to transmit.

This design also makes the library easy to integrate into any transport layer. The caller handles framing (device address, CRC) and passes only the PDU payload to the library. This matches the Modbus specification's own layering: the PDU is defined independently of the transport.

---

## Header Layering: Enforcing the Public/Private Boundary

The include directory is split into two levels:

```
Inc/
├── Public/    ← exposed to consumers of the library
│   ├── modbus.h         (public API)
│   └── modbus_types.h   (types needed to use the API)
└── Private/   ← internal to the library only
    ├── modbus_helpers.h
    ├── modbus_coils.h
    └── modbus_discrete_inputs.h
```

This boundary is enforced by CMake, not just by convention:

```cmake
target_include_directories(modbus PUBLIC  Inc/Public)
target_include_directories(modbus PRIVATE Inc/Private)
```

Consumers who link against the library can only see `Inc/Public`. The internal dispatch functions, helper utilities, and implementation details are structurally invisible. This means the internal structure can be changed freely — new submodules added, helpers refactored — without affecting any code that depends on the library.

The split also solved a real problem during development. Submodules need access to the shared types (`modbus_context_t`, enums, function pointer typedefs) but should not depend on each other or on the public API header. Extracting `modbus_types.h` gave every internal module a common dependency with no circular include chains.

---

## Function Code Dispatch by Submodule

Each group of related function codes lives in its own translation unit:

- `modbus_coils.c` — FC 0x01, 0x05, 0x0F (read coils, write single coil, write multiple coils)
- `modbus_discrete_inputs.c` — FC 0x02 (read discrete inputs)
- `modbus.c` — the top-level dispatcher only

The top-level dispatcher contains nothing but a switch statement:

```c
switch (function_code)
{
case MODBUS_FC_READ_COILS:
    return dispatch_read_coils(...);
case MODBUS_FC_READ_DISCRETE_INPUTS:
    return dispatch_read_discrete_inputs(...);
// ...
default:
    return generate_exception_response(..., MODBUS_EX_ILLEGAL_FUNCTION, ...);
}
```

Adding a new function code means adding a new file and a new case. Nothing else changes. This makes the codebase easy to extend and easy to review — a reader can understand the coils implementation without reading anything else.

---

## Validation Strategy: Defence Before Trust

Every dispatch function validates inputs before using them, in a deliberate order:

1. **Request length** — before reading any fields from the wire buffer
2. **Field value ranges** — before allocating stack memory sized by those values
3. **Cross-checks** — derived fields on the wire (such as `byte_count`) are verified against independently computed values rather than trusted directly
4. **Response capacity** — before writing any output
5. **Data access** — the context function is called last, only after everything else is known good

This order matters. In `dispatch_write_multiple_coils`, for example, the request includes a `byte_count` field that is mathematically derivable from `quantity_of_outputs`. Rather than trusting the wire value, the library computes the expected value and returns an exception response if they disagree:

```c
if (byte_count != modbus_packed_bits_to_bytes_count(quantity_of_outputs))
{
    return generate_exception_response(..., MODBUS_EX_ILLEGAL_DATA_VALUE, ...);
}
```

This defends against malformed or malicious requests without silently corrupting memory or producing undefined behaviour.

---

## No Heap Allocation

The library never calls `malloc`, `calloc`, or `free`. All temporary storage uses stack-allocated variable-length arrays:

```c
bool coils_values[quantity_of_coils];
```

This is a hard requirement for bare-metal embedded targets, where dynamic allocation is either unavailable, unreliable, or prohibited by coding standards (MISRA, DO-178C, and others forbid it). Stack usage is bounded by the Modbus specification itself — a maximum of 2000 coils or 125 registers per request — so the worst-case stack depth is known at design time.

### Known limitation: VLAs and MISRA
 
VLAs are valid C99 but are banned by MISRA-C because their size is determined at runtime, which prevents static analysers from computing worst-case stack usage at compile time. In this library the quantity field is validated against the Modbus maximum before the VLA is allocated, so the worst case is bounded — but the static analyser cannot see that guarantee.
 
For stricter environments, the right solution is not a fixed-size array but a **caller-provided buffer**:
 
```c
size_t modbus_dispatch_PDU(modbus_context_t *context,
                           const uint8_t    *wire_request,
                           uint8_t           request_length,
                           uint8_t          *wire_response,
                           uint8_t           response_capacity,
                           bool             *coil_buffer,
                           uint16_t          coil_buffer_size,
                           uint16_t         *register_buffer,
                           uint16_t          register_buffer_size);
```
 
The caller decides where the memory lives — stack, static storage, or elsewhere — and the library makes no allocation decision at all. This is the most embedded-friendly pattern: the library is a pure function over caller-owned memory, with zero allocation policy of its own.
 
The tradeoff is API complexity. For the current target (STM32F446RE, 128KB SRAM) VLAs are perfectly safe and the simpler API is the right call. The caller-provided buffer pattern is the path forward if this library is ever used on more constrained hardware or in a MISRA-compliant codebase.

---

## Inline vs. Out-of-Line Functions

Helper functions are split into two categories based on a simple rule: functions where the compiler needs to see the body to generate efficient code are marked `static inline` in the header; everything else gets a `.c` file.

The `static inline` functions are all small, called frequently, and operate on primitive types:

```c
static inline uint16_t modbus_read_u16_be(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}
```

At optimisation level `-O2` or higher, these collapse to a handful of instructions at each call site with no function call overhead. Functions with loops, branching, or non-trivial logic (`modbus_pack_bools_to_u8_array`, `generate_exception_response`) are compiled once and called normally — inlining them would increase binary size with no benefit.

---

## What This Project Demonstrates

Building a correct, portable, testable embedded library requires making a series of decisions that are not obvious from the feature description alone:

- Where does protocol logic end and hardware access begin?
- Who owns the buffers, and what happens when they are too small?
- In what order should inputs be validated, and what should each failure produce?
- How should the build system enforce architectural boundaries?
- What belongs in a header, and what belongs in a source file?

Each of these was a deliberate choice, not an accident of implementation. The result is a library that can be dropped into any C99 project, tested without hardware, extended without modifying existing code, and understood one module at a time.
