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
| 2 | Sequencer running | Set by a start press. Cleared by any fault, on the pass it is raised |
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
| 7 | Loop time | µs | Last loop pass, see below |
| 8 | Max loop time | µs | Longest loop pass since reset |

Read each position as one two-register request so both words come from the same
moment.

Loop time is measured with the DWT cycle counter, from just before the inputs are
read to the end of the pass, so it includes serving Modbus and is close to the loop
period. Both registers
saturate at 65535 µs. The max only resets with the MCU.

Operation index, which is the position in the array in
[`App/app/sequence.cpp`](../App/app/sequence.cpp). The monitor's stage list copies it
too, so all three change together:

| Index | Stage | Operation |
|---|---|---|
| 0 | Home | `HOME_Y` |
| 1 | Home | `HOME_X` |
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
after its cause goes away. All of that happens on the same loop pass the fault is
raised. Read registers 5–6 as one request so they come from the same moment.

The bit numbers are the order of `Fault` in
[`App/machine/faults.hpp`](../App/machine/faults.hpp).

| Bit | Value | Name | Raised by | Raised when |
|---|---|---|---|---|
| 0 | 1 | E-stop | `loop()` | The e-stop reads pressed, including a broken wire |
| 1 | 2 | Operation interrupted | A move | Something other than its limit switch stopped it before its target |
| 2 | 4 | Operation timed out | Homing | It didn't finish within its timeout |
| 3 | 8 | X homing failed | Homing X | The X switch still read pressed after backing off |
| 4 | 16 | Y homing failed | Homing Y | The same, for the Y switch |
| 5 | 32 | X limit hit | A move on X | The X switch read pressed before the move reached its target, in either direction |
| 6 | 64 | Y limit hit | A move on Y | The same, for the Y switch |

- **Clearing:** one start press clears every latched fault, unless the e-stop is still
  pressed. Clearing doesn't start the machine: that takes a second press, which restarts
  the cycle from operation 0. There is no remote clear.
- **Failed operation:** input register 0 stays on it until the next start.
- **Limit switch hits:** the switches are at the negative end of each axis.
  - During a move, in either direction: the move raises its axis's *limit hit* fault.
    Heading into the switch, it's a hit. Heading away, the axis should be clear of it
    after homing, so a press means a stuck or miswired switch, lost steps, or an axis
    that was never homed.
  - While homing: no fault. Homing expects the switch.
  - While the sequencer is stopped: the axis stops if it's moving toward the switch,
    but no fault is raised, because only a running operation reports one.
- **Homing failed:** the switch still reads pressed after the back-off, so it's stuck or
  miswired.

## Holding registers (FC 03 read, FC 06 / FC 16 write)

| Address | Name | Unit | Default | Range |
|---|---|---|---|---|
| 0 | X input position | mm | 0 | 0–550 |
| 1 | X dip position | mm | 250 | 0–550 |
| 2 | X output position | mm | 500 | 0–550 |
| 3 | Y up position | mm | 0 | 0–200 |
| 4 | Y down position | mm | 100 | 0–200 |
| 5 | Dwell time | ms | 2000 | 0–60000 |

- A written value takes effect the next time its operation starts. An operation
  already running keeps the value it started with.
- `Y down position` is shared by the pick, dip, and deposit steps.
- A multi-register write is checked in full before anything is applied: if any
  value is out of range, nothing changes and the server answers exception `0x04`
  (server failure). The library has no "illegal data value" path yet.
- Values live in RAM and return to the defaults on reset.
- Positions are whole millimetres from the origin set by homing. Each move converts
  its position to steps when it starts, through `Machine::SCALE_X` / `SCALE_Y`.
- The position ranges are the axis travel (`Machine::X_TRAVEL_MM`, `Y_TRAVEL_MM`).
- Positions are in mm here but the live positions (input registers 1–4) are in steps.

## Coils (FC 01 / 05 / 15)

Not mapped: every coil request returns exception `0x01` (illegal function). There
are no remote commands yet; starting and stopping stay on the physical buttons.
