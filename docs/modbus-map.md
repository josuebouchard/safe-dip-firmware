# Modbus map

Modbus TCP server on the W5500, port 502. The unit ID is not checked; it is echoed back in the response. The map lives in
[`App/Src/entrypoint.cpp`](../App/Src/entrypoint.cpp) under `=== Modbus map ===`;
keep this file in sync with it.

All addresses are zero-based protocol addresses (the PDU address field). Reading or
writing past the end of a table returns exception `0x02` (illegal data address).

## Discrete inputs (FC 02, read-only)

| Address | Name | Meaning |
|---|---|---|
| 0 | E-stop pressed | Live read of `E_STOP` |
| 1 | Start pressed | Live read of `START` |
| 2 | Sequencer running | Cleared by the e-stop, set by a start press |
| 3 | X at target | X axis is in position mode and on its target step |
| 4 | Y at target | Y axis is in position mode and on its target step |

## Input registers (FC 04, read-only)

| Address | Name | Unit | Notes |
|---|---|---|---|
| 0 | Operation index | — | Index into the cycle, see below |
| 1 | X position, high word | steps | Signed 32-bit, high word first |
| 2 | X position, low word | steps | |
| 3 | Y position, high word | steps | Signed 32-bit, high word first |
| 4 | Y position, low word | steps | |

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
