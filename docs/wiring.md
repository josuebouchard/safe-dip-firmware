# Wiring — NUCLEO-F446RE

Board: NUCLEO-F446RE (STM32F446RET6, LQFP64). Pin assignments come from
[`nucleo-project.ioc`](../nucleo-project.ioc); edit them in STM32CubeMX and regenerate.

## Pinout

| Signal | Label | Arduino | Header | MCU pin | Mode |
|---|---|---|---|---|---|
| X step | `PULSE_x` | A0 | CN8 | PA0 | TIM2_CH1, output compare toggle, **open-drain** |
| Y step | `PULSE_y` | A1 | CN8 | PA1 | TIM2_CH2, output compare toggle, **open-drain** |
| X direction | `DIR_x` | A3 | CN8 | PB0 | GPIO output, **open-drain**, idle released |
| Y direction | `DIR_y` | A4 | CN8 | PC1 | GPIO output, **open-drain**, idle released |
| Emergency stop | `E_STOP` | D4 | CN9 | PB5 | GPIO input, internal pull-up, active low |
| Start button | `START` | D5 | CN9 | PB4 | GPIO input, internal pull-up, active low |
| Effector | `EFFECTOR` | D6 | CN9 | PB10 | GPIO output, push-pull, idle low |
| W5500 RST | `W5500_RST` | D9 | CN5 | PC7 | GPIO output, push-pull, idle high |
| W5500 CS | `SPI_CS` | D10 | CN5 | PB6 | GPIO output, push-pull, idle high |
| W5500 MOSI | — | D11 | CN5 | PA7 | SPI1_MOSI |
| W5500 MISO | — | D12 | CN5 | PA6 | SPI1_MISO |
| W5500 SCK | — | D13 | CN5 | PA5 | SPI1_SCK, 42 Mbit/s |
| Debug UART | `USART_TX` / `USART_RX` | — | — | PA2 / PA3 | USART2 to the ST-LINK VCP, 115200 8N1 |
| User button | `B1` | — | — | PC13 | on-board blue button, polled (no EXTI handler) |

Free on the headers: D2, D3 (SWO), D7, D8, D14, D15, A2, A5.

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
- Output compare polarity is `TIM_OCPOLARITY_LOW`, so a stopped axis parks with the pulse pin
  released. One step is counted per active (falling) edge.
- No internal pull-ups on these four pins: the driver's LED and its series resistor to 5 V are the
  pull-up. An internal pull-up to 3.3 V would fight that rail.
- If the driver inputs need more than 5 V, or the anode rail is 12/24 V, **do not wire them
  directly** — no STM32 pin tolerates that. Use series resistors sized for the rail or a buffer.
- If an axis turns the wrong way, flip `electrical_state_clockwise` for that axis in
  `App/Src/entrypoint.cpp`.

## 5 V tolerance

Verified against the STM32F446 datasheet (DS10693), Table 10 "pin and ball descriptions":

- **FT (5 V tolerant):** PA0, PA1, PB0, PC1, PB4, PB5, PB10, PA8, PA9, PA10, PC0.
- **TTa (NOT 5 V tolerant):** PA4 (A2) and PA5 (D13) — both are DAC outputs. Never feed either a
  5 V signal. This is why `DIR_x` sits on A3 rather than A2.

## Buttons

`E_STOP` and `START` are active low: wire each switch between its pin and GND. The internal
pull-up holds the pin high when the switch is open. For a real emergency stop, use a
normally-closed contact so that a broken wire reads as pressed, and invert the test in
`App/Src/entrypoint.cpp`.

## Notes

- LD2, the green user LED, shares PA5 with SPI1 SCK, so it flickers during Ethernet traffic.
- D0/D1 (PA3/PA2) are wired to the ST-LINK virtual COM port, not to the Arduino header pins —
  `printf` output goes there at 115200 baud.
- TIM2 runs at 1 MHz (84 MHz APB1 timer clock / 84), 32-bit free-running.
