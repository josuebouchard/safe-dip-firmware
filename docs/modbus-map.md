# Modbus map

Modbus TCP server on the W5500, port 502. The unit ID is not checked; it is echoed back in the response. The map lives in
[`App/app/modbus_map.cpp`](../App/app/modbus_map.cpp); keep this file in sync with it.

All addresses are zero-based protocol addresses (the PDU address field). Reading or
writing past the end of a table returns exception `0x02` (illegal data address).

## Discrete inputs (FC 02, read-only)

| Address | Name | Meaning |
|---|---|---|
| 0 | E-stop pressed | 1 while pressed (`E_STOP` low), as read at the start of the loop pass |
| 1 | Start pressed | 1 while pressed (`B1` low), as read at the start of the loop pass |
| 2 | Sequencer running | Cleared by any fault, set by a start press |
| 3 | X at target | X axis is in position mode and on its target step |
| 4 | Y at target | Y axis is in position mode and on its target step |
| 5 | Effector engaged | Commanded state of `EFFECTOR`, not feedback from the magnet |
| 6 | Fault active | 1 while any fault is latched, see [Faults](#faults) |

## Input registers (FC 04, read-only)

| Address | Name | Unit | Notes |
|---|---|---|---|
| 0 | Operation index | — | Index into the cycle, see below |
| 1 | X position, high word | steps | Signed 32-bit, high word first |
| 2 | X position, low word | steps | |
| 3 | Y position, high word | steps | Signed 32-bit, high word first |
| 4 | Y position, low word | steps | |
| 5 | Faults active | bits | Every latched fault, see [Faults](#faults) |
| 6 | First fault | bit | The fault that stopped the machine, 0 if none |
| 7–13 | Fault sources | — | One per fault, in bit order |

Read each position as one two-register request so both words come from the same
moment.

Operation index:

| Index | Stage | Operation |
|---|---|---|
| 0 | Home | `MOVE_Y_UP` |
| 1 | Home | `MOVE_X_HOME` |
| 2 | Pick | `MOVE_Y_DOWN` |
| 3 | Pick | `EFFECTOR_ENGAGE` |
| 4 | Pick | `MOVE_Y_UP` |
| 5 | Dip | `MOVE_X_DIP` |
| 6 | Dip | `MOVE_Y_DOWN` |
| 7 | Dip | `DWELL` |
| 8 | Dip | `MOVE_Y_UP` |
| 9 | Deposit | `MOVE_X_OUTPUT` |
| 10 | Deposit | `MOVE_Y_DOWN` |
| 11 | Release | `EFFECTOR_DISENGAGE` |
| 12 | Release | `MOVE_Y_UP` |

## Faults

A fault stops the sequencer and both axes, engages the effector, and stays latched
after its cause goes away. The source is `0xFFFF` when a fault isn't latched or has no
source. Read registers 5–13 as one request so they all come from the same moment.

| Bit | Value | Source register | Name | Raised when | Source |
|---|---|---|---|---|---|
| 0 | 1 | 7 | E-stop | The e-stop reads pressed, including a broken wire | None |
| 1 | 2 | 8 | Operation interrupted | A move was stopped before reaching its target | Operation index |
| 2 | 4 | 9 | Operation timed out | Homing didn't finish within its timeout | Operation index |
| 3 | 8 | 10 | Operation failed | An operation aborted without giving a reason | Operation index |
| 4 | 16 | 11 | Home switch stuck | The switch still read pressed after backing off | Operation index |
| 5 | 32 | 12 | Position drift | The switch was hit too far from where the count expected it | Operation index |
| 6 | 64 | 13 | Unexpected reset | The MCU restarted from a software, watchdog, low-power or brownout reset | Reset flags |

- **Clearing:** one start press clears every latched fault, unless the e-stop is still
  pressed. Clearing doesn't start the machine: that takes a second press, which restarts
  the cycle from operation 0. There is no remote clear.
- **Operation index:** the same numbering as input register 0, which also stays on the
  failed operation until the next start.
- **Limit switch hits:** a hit while the sequencer is stopped stops the axis but raises
  no fault, because only a running operation reports one.
- **Position drift:** homing checks against the origin set by that axis's previous home,
  so the first home after a reset can't raise it. Its next home sets a new origin.
- **Home switch stuck:** a switch that reads pressed far from its expected position raises
  position drift first, then this fault on the next home.

Reset flags are bits 24–31 of `RCC_CSR`, shifted down:

| Bit | Value | Flag |
|---|---|---|
| 1 | 2 | Brownout, also set by a power-on |
| 2 | 4 | Reset pin, set by every reset |
| 3 | 8 | Power-on |
| 4 | 16 | Software, including most debugger resets |
| 5 | 32 | Independent watchdog |
| 6 | 64 | Window watchdog |
| 7 | 128 | Low power |

A power-on or the reset button raises no fault. Flashing with a debugger usually does,
so expect to press start once to clear it on the bench.

## Holding registers (FC 03 read, FC 06 / FC 16 write)

| Address | Name | Unit | Default | Range |
|---|---|---|---|---|
| 0 | X input position | steps | 0 | 0–1000 |
| 1 | X dip position | steps | 500 | 0–1000 |
| 2 | X output position | steps | 1000 | 0–1000 |
| 3 | Y up position | steps | 0 | 0–500 |
| 4 | Y down position | steps | 500 | 0–500 |
| 5 | Dwell time | ms | 2000 | 0–60000 |

- A written value takes effect the next time its operation starts. An operation
  already running keeps the value it started with.
- `Y down position` is shared by the pick, dip, and deposit steps.
- A multi-register write is checked in full before anything is applied: if any
  value is out of range, nothing changes and the server answers exception `0x04`
  (server failure). The library has no "illegal data value" path yet.
- Values live in RAM and return to the defaults on reset.
- The position ranges are placeholders equal to the current maximum targets
  (`X_MAX_STEPS`, `Y_MAX_STEPS`); set them from the measured axis travel.

## Coils (FC 01 / 05 / 15)

Not mapped: every coil request returns exception `0x01` (illegal function). There
are no remote commands yet; starting and stopping stay on the physical buttons.
