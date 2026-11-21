# Safe Dip — firmware

Firmware for **Safe Dip**, an automated dipping station: a two-axis gantry that picks up a part, lowers it into a bath for a set time, and deposits it, without anyone standing over it. It runs on an STM32 microcontroller and reports its state over Ethernet with Modbus TCP, so a PLC or a computer can watch it.

Safe Dip is the senior design capstone of Josué Bouchard and Adriano Duque Mena (ECET, Valencia College, Fall 2026). **The firmware in this repository was written by Josué Bouchard.** Adriano designed and built the mechanical side (the CAD, the 3D-printed parts, and the frame), and the two worked together on the electrical build, the testing, and the report.

- Project website, with the design, the test results, and the weekly log: <https://safe-dip.vercel.app>
- Final report (PDF): <https://safe-dip.vercel.app/build/report-and-presentation/>

## What it does

One cycle, started by a part-ready signal:

1. **Home** both axes against their limit switches.
2. **Wait** for the part-ready trigger.
3. **Pick:** lower the Y axis, switch the electromagnet on, and lift.
4. **Dip:** move the X axis over the bath, lower the part, wait the dwell time, and lift.
5. **Deposit and release:** move to the output position, lower, switch the electromagnet off, and lift.

The cycle is 14 operations in a list, and the sequencer runs one step of it on every pass of the main loop. An emergency stop cuts the motor supply in hardware, independently of the firmware, and the firmware adds latched faults that put the machine in a safe state (axes stopped, the electromagnet kept engaged so a part is not dropped into the bath) until the operator presses Start.

## Hardware

| Part | Used for |
|---|---|
| NUCLEO-F446RE (STM32F446RET6) | the controller |
| 2 × NEMA 23 stepper motors, 2 × DM542TE drivers, 24 V supply | the X axis (belt) and the Y axis (lead screw) |
| Electromagnet and relay module, 12 V supply | holding the part |
| W5500 Ethernet module | Modbus TCP over SPI |
| Limit switches, emergency-stop button, Start button, part-ready trigger | inputs |

The pin assignments are in [`docs/wiring.md`](docs/wiring.md).

## Software highlights

- **Cooperative and nonblocking.** There is one main loop and no RTOS. Every module does a bounded amount of work per pass and returns, so the emergency stop, the limit switches, and Modbus stay responsive while an axis moves.
- **Step pulses from hardware timers.** The STEP signals are produced by timer output-compare interrupts, not by the main loop. A trapezoidal speed planner shapes each move.
- **The cycle is data.** Each step is an operation behind one small interface, so a step can be added or reordered without changing the sequencer.
- **Safety in layers.** The limit switches are debounced, and a latched fault word records every abnormal condition, remembering the first one as the cause.
- **Modbus TCP server** on the W5500: machine state, positions, and faults to read, and the recipe (positions, dwell time) to write. The Modbus protocol handling is a separate library in this repository, written in C99.
- **Layered C++.** The application is split into layers (`drivers`, `control`, `machine`, `operations`, `sequencer`, `app`), and CMake enforces the dependencies between them: including a header from a layer you do not depend on is a build error.
- **A Modbus monitor** (a small Go program with a web page, and unit tests for its Modbus decoding) that shows the cycle, the axis positions, the faults, and the loop time during testing.

## Documentation

| File | What it covers |
|---|---|
| [`App/README.md`](App/README.md) | How the application is organized: the layers, the mental model (inputs, capabilities, operations, the sequencer), and what one pass of the main loop does. Start here to read the code. |
| [`docs/wiring.md`](docs/wiring.md) | The NUCLEO pinout, which signal goes to which pin and driver, and how the relays and inputs behave when powered and unpowered. |
| [`docs/modbus-map.md`](docs/modbus-map.md) | The Modbus TCP register map: discrete inputs, input registers, holding registers, the operation index, and the faults. |
| [`Libraries/modbus/readme.md`](Libraries/modbus/readme.md) | The Modbus library: what it is, the function codes it supports, and how to use it. |
| [`Libraries/modbus/docs/architecture.md`](Libraries/modbus/docs/architecture.md) | Why the Modbus library is built the way it is: the decisions behind separating the protocol from the hardware. |
| [`Libraries/modbus/docs/retrospect.md`](Libraries/modbus/docs/retrospect.md) | A personal retrospective on building the library, with the lessons learned. |

## Building

The project uses CMake with Ninja and the `starm-clang` toolchain from STM32CubeCLT. The CubeMX project is [`nucleo-project.ioc`](nucleo-project.ioc); pins are edited there and regenerated.

```sh
cmake --preset Release
cmake --build --preset Release
```

To watch a running machine, start the monitor from `tools/modbus_monitor` with `go run .` and open <http://127.0.0.1:8000>.

## Repository layout

```
App/            the application, in layers (see App/README.md)
Core/           STM32CubeMX-generated startup and peripheral code
Drivers/        STM32 HAL and CMSIS (from ST)
Libraries/      the Modbus library (mine) and the WIZnet Ethernet driver
docs/           wiring and the Modbus map
tools/          the Modbus monitor
```

## License

Copyright (c) 2026 Josué Bouchard. All rights reserved.

This code is publicly visible for viewing and portfolio purposes only. You may not copy, modify, distribute, or use this code without explicit written permission from the copyright owner. See [`LICENSE`](LICENSE).

The code in `Drivers/` (STM32 HAL and CMSIS, from STMicroelectronics) and `Libraries/WIZNET/` (the WIZnet ioLibrary) is third-party and stays under its own licenses, which are included alongside it.
