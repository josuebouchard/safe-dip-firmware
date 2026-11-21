# Wiring — NUCLEO-F446RE

Board: NUCLEO-F446RE (STM32F446RET6, LQFP64). Pin assignments come from
[`nucleo-project.ioc`](../nucleo-project.ioc); edit them in STM32CubeMX and regenerate.
The driver objects that use these pins are created once in
[`App/drivers/hardware.cpp`](../App/drivers/hardware.cpp) and shared by every entrypoint.

## Pinout

| Device | Signal | Label | Arduino | Header | MCU pin | Mode |
|---|---|---|---|---|---|---|
| X stepper driver | Step | `PULSE_x` | A0 | CN8 | PA0 | TIM2_CH1, output compare toggle, **open-drain** |
| Y stepper driver | Step | `PULSE_y` | A1 | CN8 | PA1 | TIM2_CH2, output compare toggle, **open-drain** |
| X stepper driver | Direction | `DIR_x` | A3 | CN8 | PB0 | GPIO output, **open-drain**, idle released |
| Y stepper driver | Direction | `DIR_y` | A4 | CN8 | PC1 | GPIO output, **open-drain**, idle released |
| Piece trigger | Contact | `TRIGGER` | A5 | CN8 | PC0 | GPIO input, internal pull-up, active low (low = piece present) |
| Emergency stop button | Contact | `E_STOP` | D4 | CN9 | PB5 | GPIO input, internal pull-down, NC contact to 3.3 V (low = pressed) |
| Start button | Contact | `START` | D5 | CN9 | PB4 | GPIO input, internal pull-up, active low |
| Effector relay | Input | `EFFECTOR` | D6 | CN9 | PB10 | GPIO output, **open-drain**, released = engaged |
| X home switch | Contact | `X_HOME` | D7 | CN9 | PA8 | GPIO input, internal pull-up, NC contact (high = home) |
| Y home switch | Contact | `Y_HOME` | D8 | CN5 | PA9 | GPIO input, internal pull-up, NC contact (high = home) |
| W5500 Ethernet | Reset | `W5500_RST` | D9 | CN5 | PC7 | GPIO output, push-pull, idle high |
| W5500 Ethernet | Chip select | `SPI_CS` | D10 | CN5 | PB6 | GPIO output, push-pull, idle high |
| W5500 Ethernet | MOSI | — | D11 | CN5 | PA7 | SPI1_MOSI |
| W5500 Ethernet | MISO | — | D12 | CN5 | PA6 | SPI1_MISO |
| W5500 Ethernet | SCK | — | D13 | CN5 | PA5 | SPI1_SCK, 42 Mbit/s |
| ST-LINK virtual COM port | TX / RX | `USART_TX` / `USART_RX` | — | — | PA2 / PA3 | USART2, 115200 8N1 |
| On-board user button (B1) | Contact | `B1` | — | — | PC13 | blue button, polled (no EXTI handler) |

Free on the headers: D2, D3 (SWO), A2. A2 (PA4) is not 5 V tolerant and has no timer channel.

## Stepper drivers (common anode, 5 V)

The MCU **sinks** current; it never sources it. Each driver input is an opto-isolator LED
whose anode sits on 5 V, so the MCU pin pulls the cathode down to light it.

```
  Driver                        Nucleo
  PUL+  ──── +5 V
  PUL-  ──────────────────────  A0 (X) / A1 (Y)     open-drain
  DIR+  ──── +5 V
  DIR-  ──────────────────────  A3 (X) / A4 (Y)     open-drain
  GND   ──────────────────────  GND
```

- **Pin released (high-Z)** → no current → opto off. **Pin driven low** → current flows → opto on.
- Output compare polarity is `TIM_OCPOLARITY_LOW`, so one step is counted per active (falling)
  edge while the axis moves.
- **Idle and boot:** a stopped channel is released, so the opto stays off. `HAL_TIM_OC_Stop_IT`
  clears `CCxE`, and so does `MX_TIM2_Init` at boot. RM0390 defines that state as "Output Disabled
  (not driven by the timer) — OCx=0, OCx_EN=0": the timer stops driving the pad and leaves it
  tri-stated. TIM2 has no `BDTR.OSSR`, the only bit that would make a disabled output drive a level.
  On an open-drain pin, tri-stated is the same as released, and the driver's pull-up to 5 V holds the
  line high. The `FORCED_INACTIVE` mode that `stop_interrupt()` sets never reaches the pin while
  `CCxE` is off; it is harmless. Never use `FORCED_ACTIVE` for parking: with the output enabled it
  would hold the opto on.
- This relies on open-drain. On a push-pull pin the same tri-state would leave the line floating
  and it would need a pull resistor.
- Bench confirmation: with no move running, A0/A1 should read ~5 V against GND (released). ~0.3 V
  would mean driven low. The general-purpose timer wording comes from a search snippet of the
  manual rather than a full extract, so this measurement is the final word.
- No internal pull-ups on these four pins: the driver's LED and its series resistor to 5 V are the
  pull-up. An internal pull-up to 3.3 V would fight that rail.
- If the driver inputs need more than 5 V, or the anode rail is 12/24 V, **do not wire them
  directly** — no STM32 pin tolerates that. Use series resistors sized for the rail or a buffer.
- If an axis turns the wrong way, flip `electrical_state_clockwise` for that axis in
  `App/drivers/hardware.cpp`; the fix then applies to every entrypoint.

## Effector relay (active low, 12 V on NC)

The effector is a plain 12 V electromagnet: it holds while powered and lets go at 0 V. Its safe state
is engaged, so the 12 V supply runs through the relay's **normally-closed** contact. The magnet is
powered whenever the relay coil is off, and the coil is only on while the MCU actively pulls `IN`
low.
The relay input is active low and the MCU **sinks** 4 mA (measured) to drive it, the same way it drives
the stepper optos.

**Control side (5 V):**

| Relay module | Connects to | Notes |
|---|---|---|
| VCC | Nucleo 5V pin | Same supply as the MCU, so both lose power together |
| IN | D6 (PB10) | Open-drain, no pull |
| GND | Nucleo GND | |

**Load side (12 V):**

| Terminal | Connects to | Notes |
|---|---|---|
| NC | +12 V | Closed while the coil is off |
| COM | Effector + | |
| NO | — | Unused |
| Effector − | 12 V supply GND | |

| PB10 | Written by | Relay coil | Contact | Effector |
|---|---|---|---|---|
| Released (high-Z) | `GPIO_PIN_SET`, or MCU in reset / unpowered | off | NC closed | 12 V → **engaged** |
| Driven low | `GPIO_PIN_RESET` | on | NC open | 0 V → disengaged |

- `EffectorDriver::engage()` writes `GPIO_PIN_SET`, which on an open-drain pin means released;
  `disengage()` writes `GPIO_PIN_RESET`, which drives the pin low.
- **Fail-safe:** the magnet engages whenever the coil drops out, whatever the reason: the MCU is
  held in reset, loses power, or never configured the pin; the relay module loses its 5 V; the
  `IN` wire breaks.
- **Boot:** after reset every GPIO except the debug pins is a floating input, so PB10 is high-Z
  until `MX_GPIO_Init` runs. `MX_GPIO_Init` writes `GPIO_PIN_SET` to the output register *before*
  switching the pin to open-drain output, so there is no low glitch. `setup()` then calls
  `disengage()`, so the magnet holds through the reset and releases during boot. If a fault is
  latched at boot, the first `loop()` pass engages it again.

**What is not covered:**

- **Loss of the 12 V supply** drops the load, whatever the relay does. So does a broken wire to the
  magnet. Only a permanent-magnet (power-off holding) type or a backup supply covers this.
- **A welded relay contact** (NC stuck closed) keeps the magnet on, so `disengage()` has no effect.
  This can only be detected with feedback from the effector.
- **Flyback diode (required):** the electromagnet is a large inductor. Put a diode across it,
  cathode to `effector +`. Without it, opening the contact makes the coil's stored energy arc across
  the NC contact and spike the 12 V line. A plain diode slows the release; a diode in series with a
  zener releases faster. The module's own diode protects the relay coil, not the magnet.

**Electrical checks (STM32F446RE):**

- **Effector pin must be FT.** The module pulls `IN` up to its own 5 V through the opto LED and resistor, so
  the released pin sits close to 5 V. PB10 is FT (DS10693, Table 10). The internal pull-up must stay
  off, because a pull-up to 3.3 V would fight the 5 V. A push-pull output would clamp the line at
  3.3 V and leave the opto partly lit, which is why the pin must be open-drain.
- FT pins are only rated for $V_{DD}$ + 4 V (max 5.5 V) on the input, which drops to 4 V when
  $V_{DD}$ = 0. Power the module from the Nucleo's **5V pin** so that it loses power together with
  the MCU. Do not power it from a separate 5 V supply that can stay up while the board is off.
- Sink current: **4 mA measured on the bench**, well inside the $V_{OL}$ ≤ 0.4 V at 8 mA
  rating.
- The module is **active low**: `IN` is pulled up to VCC internally, and pulling it low turns the
  relay on. That pull-up is why a released pin turns the relay off.

## 5 V tolerance

Verified against the STM32F446 datasheet (DS10693), Table 10 "pin and ball descriptions":

- **FT (5 V tolerant):** PA0, PA1, PB0, PC1, PB4, PB5, PB10, PA8, PA9, PA10, PC0
- **TTa (NOT 5 V tolerant):** PA4 (A2) and PA5 (D13) — both are DAC outputs. Never feed either a
  5 V signal. This is why `DIR_x` sits on A3 rather than A2.

## Home switches (NC to GND, 3.3 V pull-up)

There is one home switch per axis, at the home end of travel. `X_HOME` (PA8) and `Y_HOME` (PA9)
are configured in the `.ioc` as GPIO inputs with the internal pull-up on; `main.h` defines
`X_HOME_Pin` / `Y_HOME_Pin`. Each switch is wired between its pin and GND with the **normally-closed** contact. While the axis
is clear, the closed contact ties the pin to GND; hitting the switch opens it and the pull-up
pulls the pin high. A broken wire or an unplugged connector also reads high, so it stops the axis
instead of letting it crash.

| Switch terminal | Connects to | Notes |
|---|---|---|
| C (common) | Nucleo GND | |
| NC | `X_HOME` / `Y_HOME` pin | Closed while the axis is clear |
| NO | — | Unused |

| Pin reads | Switch | Meaning |
|---|---|---|
| Low | Closed | Axis clear |
| High | Open | **Home reached**, or wire broken |

- **Pull-up to 3.3 V, never 5 V.** A 5 V pull-up would exceed the FT rating ($V_{DD}$ + 4 V) while
  the MCU is unpowered and gives little extra margin. The cable only carries GND and the signal,
  so a short to the frame cannot damage anything.
- **Noise:** each switch cable is a twisted pair (signal + GND), routed away from the stepper
  wiring, and the software debounce below filters what still gets through. The internal ~40 kΩ
  pull-up is weak, so if false hits remain, add a 10 nF capacitor from the pin to GND at the board.
- **Software:** `LimitFilter` (`App/debounce/limit_filter.hpp`) counts a press only after the
  pin has read high for `press_ms` without a break; a release counts at once. Raise the time if noise
  still gets through, keeping in mind the axis runs on past the switch for that long.
- Both pins are FT, and both also carry TIM1 channels (CH1, CH2) if one is ever needed for capture.
- **No switch at the far end:** travel away from home is limited only in software. Home the axis
  first, then reject any move past the known travel length.

## Piece trigger (active low, 3.3 V pull-up)

`TRIGGER` (PC0, A5) is a GPIO input with the internal pull-up on. The sequence's
`WAIT_FOR_PIECE` step holds the pick cycle until the pin reads low. Wire a dry contact or an
open-collector/open-drain sensor output between the pin and GND: it pulls the pin low when a piece
is present and releases it otherwise.

| Pin reads | Meaning |
|---|---|
| Low | Piece present, the cycle continues |
| High | No piece, or sensor unplugged or wire broken: the cycle waits |

- **Fails safe:** a broken wire reads high, so the machine waits instead of picking at nothing.
- Pull-up to 3.3 V, never 5 V, for the same reason as the home switches. PC0 is FT, so a sensor
  output that swings to 5 V is tolerated while the MCU is powered, but a sourcing (PNP) sensor needs
  a divider or a buffer, since it would fight the pull-up.
- **Software:** read once per loop pass in `read_inputs()` (`App/app/entrypoint.cpp`), and exposed on
  Modbus as discrete input 0. There is no debounce: add a filter if the sensor chatters.

## Buttons

`START` is active low: wire the switch between its pin and GND. The internal pull-up holds the
pin high when the switch is open.

`E_STOP` is low when pressed, and the internal pull-down holds it low when nothing drives it. Wire
the **normally-closed** contact between 3.3 V and the pin: while the button is released the closed
contact holds the pin high, and pressing the button or breaking the wire lets the pull-down read
low, which `App/app/entrypoint.cpp` treats as pressed. Use 3.3 V, never 5 V (PB5 is FT, but the
rating drops to 4 V when the MCU is unpowered).

## Notes

- LD2, the green user LED, shares PA5 with SPI1 SCK, so it flickers during Ethernet traffic.
- D0/D1 (PA3/PA2) are wired to the ST-LINK virtual COM port, not to the Arduino header pins —
  `printf` output goes there at 115200 baud.
- TIM2 runs at 1 MHz (84 MHz APB1 timer clock / 84), 32-bit free-running.
