#include "entrypoint.hpp"

#include "button_press.hpp"
#include "ethernet_if.h"
#include "hardware.hpp"
#include "inputs.hpp"
#include "machine.hpp"
#include "modbus_map.hpp"
#include "modbus_tcp_task.h"
#include "tick.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "w5500.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

constexpr std::uint8_t ECHO_SOCKET = 0;
constexpr std::uint16_t ECHO_PORT = 502;

static modbus_tcp_task_t modbus_task;

static Machine machine;
static modbus_context_t modbus_ctx = modbus_map_make_context(machine);

static ButtonPress start_button;

// The only place that knows pins and polarity.
static Inputs read_inputs() noexcept
{
    return Inputs{
        .e_stop_pressed = (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET),
        .start_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET),
        .limit_x_pressed = (HAL_GPIO_ReadPin(X_HOME_GPIO_Port, X_HOME_Pin) == GPIO_PIN_SET),
        .limit_y_pressed = (HAL_GPIO_ReadPin(Y_HOME_GPIO_Port, Y_HOME_Pin) == GPIO_PIN_SET),
    };
}

static void enter_safe_state() noexcept
{
    machine.sequencer.stop();
    machine.controller_x.stop_immediately();
    machine.controller_y.stop_immediately();
    hardware::effector.engage();
}

// Counts CPU cycles from `start_cycles` on the DWT counter. Unsigned subtraction stays
// correct across its wrap, every ~51 s at 84 MHz.
static void record_loop_time(std::uint32_t start_cycles) noexcept
{
    const std::uint32_t elapsed_us = (DWT->CYCCNT - start_cycles) / (SystemCoreClock / 1000000U);
    machine.loop_time_us = static_cast<std::uint16_t>(std::min<std::uint32_t>(elapsed_us, UINT16_MAX));
    machine.loop_time_max_us = std::max(machine.loop_time_max_us, machine.loop_time_us);
}

void setup()
{
    const uint32_t now = HAL_GetTick();

    ethernet_w5500_init(); // Initalize ethernet

    uint8_t version = getVERSIONR();
    std::printf("W5500 version register: 0x%02X (expect 0x04)\r\n", version);

    modbus_tcp_task_init(&modbus_task, ECHO_SOCKET, ECHO_PORT, &modbus_ctx);

    // Initialize stepper controllers
    machine.controller_x.initialize(now);
    machine.controller_y.initialize(now);

    // Initialize safe defaults
    hardware::effector.disengage();

    // Start the DWT cycle counter used to time the loop
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void loop()
{
    const std::uint32_t start_cycles = DWT->CYCCNT;

    machine.inputs = read_inputs();
    const Inputs &inputs = machine.inputs;

    const uint32_t now = HAL_GetTick();
    const bool start_press = start_button.update(inputs.start_pressed, now);

    if (inputs.limit_x_pressed && machine.controller_x.current_speed_signed() < 0)
    {
        machine.controller_x.stop_immediately();
    }

    if (inputs.limit_y_pressed && machine.controller_y.current_speed_signed() < 0)
    {
        machine.controller_y.stop_immediately();
    }

    if (inputs.e_stop_pressed)
    {
        machine.faults.raise(Fault::e_stop);
    }

    if (start_press && !inputs.e_stop_pressed)
    {
        if (!machine.sequencer.is_running())
        {
            // They weren't processed while stopped, so their last tick is stale.
            machine.controller_x.initialize(now);
            machine.controller_y.initialize(now);
        }

        // Only clears: a separate press starts the machine, so one press can't do both.
        if (machine.faults.any())
        {
            machine.faults.clear();
        }
        else if (!machine.sequencer.is_running())
        {
            machine.sequencer.restart();
        }
    }

    if (!machine.faults.any())
    {
        // Operations raise their own faults into this, and the check below acts on
        // them in this same pass.
        Tick tick{.now_ms = now, .inputs = machine.inputs, .faults = machine.faults};
        machine.sequencer.step(tick);
    }

    if (machine.faults.any())
    {
        enter_safe_state();
    }
    else
    {
        machine.controller_x.process(now);
        machine.controller_y.process(now);
    }

    // Modbus is left out: it's the part we don't control.
    record_loop_time(start_cycles);

    modbus_tcp_task_run(&modbus_task);
}
