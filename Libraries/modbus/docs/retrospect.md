# Lessons Learned — Modbus Library

Personal retrospective on building a Modbus PDU dispatcher for STM32 in C. Primarily written for future-me, but hopefuly you may find some value too.

---

## Project Structure and Modularization

### Split types early, not late

The first version of `modbus.h` held everything: enums, typedefs, function pointer types, structs, and the public API. When the time came to modularize, every submodule needed the types but not the API, and the API needed the types but not the submodules. This created an apparent circular dependency.

The fix was straightforward once identified: extract a `modbus_types.h` that has no dependencies on other modbus headers. Everything can include it without side effects. The rule generalizes: **if a circular dependency appears, the depended-on things belong in a lower-level header that neither side owns.**

### Enforce the public/private boundary in CMake, not just by convention

Using `target_include_directories` with `PUBLIC` and `PRIVATE` makes the boundary structural rather than informal:

```cmake
target_include_directories(modbus PUBLIC  Inc/Public)
target_include_directories(modbus PRIVATE Inc/Private)
```

Consumers physically cannot include internal headers without reaching outside the exposed include path. This is better than relying on people knowing which headers are internal.

### Prefix everything consistently

All public-facing types, functions, and files use the `modbus_` prefix. This prevents name collisions with other libraries and makes grep useful. The internal dispatch functions (`dispatch_read_coils`, etc.) were left without the prefix since they live behind the private include boundary — acceptable, but worth revisiting if the project grows.

### Know when to stop restructuring and start implementing

There's a real risk of spending time on structure at the expense of implementation. The structure reveals its own weaknesses under real use — problems that aren't visible when reasoning in the abstract. A useful heuristic: once you have one real implementation that validates the structure (here, `dispatch_read_discrete_inputs`), stop restructuring and implement the rest. Clean up in a single pass at the end.

---

## Embedded Systems Considerations

### Protocol logic and hardware must be fully decoupled

The entire library has zero platform dependencies. It takes raw byte buffers in, writes raw byte buffers out, and calls user-provided function pointers for data access. This means:

- The library compiles and runs on the host machine for testing
- The same binary can be tested with Unity without any STM32 toolchain
- Users can point the function pointers at any hardware, peripheral, or memory region

This was achieved through `modbus_context_t` holding function pointers rather than the library calling any HAL or peripheral directly. **Any embedded library that can be designed this way, should be.**

### No heap allocation is a hard constraint, not a preference

Variable Length Arrays (VLAs) (ex: `bool coils[quantity_of_coils]`) are used for temporary stack buffers sized at runtime. This is intentional — `malloc` is either unavailable or undesirable on bare-metal targets. The implication is that stack usage scales with request size, which is bounded by the Modbus spec at 2000 coils / 125 registers. This is known and acceptable.

### VLAs are fine for now, but know the exit path

MISRA bans VLAs because their size is a runtime value — static analysers cannot compute worst-case stack usage when the size depends on external input. The quantity field is validated before the VLA is allocated, so the worst case is actually bounded, but the tooling can't prove that.

The cleanest solution for stricter environments is not a fixed-size array but letting the caller provide the buffer:

```c
size_t modbus_dispatch_PDU(...,
                           bool    *coil_buffer,
                           uint16_t coil_buffer_size);
```

This pushes the allocation decision entirely to the caller — they can use stack, static storage, or whatever the application requires. The library becomes a pure function over caller-owned memory with no allocation policy of its own. It also makes the worst-case memory usage visible at the call site, which is exactly what MISRA requires.

The tradeoff is a more complex API. On the STM32F446RE with 128KB of SRAM the VLAs are safe and the simpler API is the right call. If this library ever targets more constrained hardware or needs to be MISRA-compliant, the caller-provided buffer pattern is the right path forward — and it's worth knowing that before starting, not after.

### Validate in the right order

For write operations with variable-length payloads (`dispatch_write_multiple_coils`), validation order matters:

1. Validate quantity range first — before allocating a VLA sized by quantity
2. Validate `byte_count` against `modbus_packed_bits_to_bytes_count(quantity)` — treat wire fields as cross-checks, not sources of truth
3. Unpack the payload — only after both quantity and byte_count are known good
4. Check response capacity
5. Call the context function

Getting this wrong means unpacking into a VLA of invalid size, or silently returning `0` instead of a proper exception response.

### Wire fields that are derivable are still worth validating

`byte_count` in a write-multiple request is fully derivable from `quantity` via `ceil(quantity / 8)`. The spec includes it on the wire as a convenience field for receivers. The correct approach is to treat the wire value as a cross-check: compute the expected value independently and return `ILLEGAL DATA VALUE` if they don't match. Never use a wire field as the sole source of truth when you can verify it.

---

## Estimation and Project Scoping

### "Implement Modbus" is not a task, it is a category

The description "implement a Modbus server library" sounds like writing a switch statement. What it actually involves:

- A protocol parser (endianness, bit packing, field extraction by byte offset)
- A memory-safe buffer handler (unknown buffer sizes, no ownership, silent failure modes)
- A spec implementation (implicit constraints the spec doesn't fully enumerate)
- An embedded-friendly API (no heap, error signaling through return values only)

Any one of these is straightforward. All four at once is genuinely complex. **The gap between the description and the reality is where estimation fails.**

### The spec describes the happy path; you have to reason about the rest

The Modbus spec clearly defines what a valid request looks like. It is much less clear about:

- What to do when `byte_count` on the wire doesn't match `quantity`
- What to do when `request_length` is shorter than the expected PDU size
- What to return when the response buffer is too small to write even an exception

These decisions required reasoning beyond the spec. They also took disproportionate time relative to the "implementation" work. Budget time for this class of work explicitly — call it **error path design** — it is not covered by time estimates for the happy path.