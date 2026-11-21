#include "entrypoint.hpp"

#include "button_press.hpp"
#include "ethernet_if.h"
#include "hardware.hpp"
#include "inputs.hpp"
#include "limit_filter.hpp"
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
#include <stm32f4xx_hal_gpio.h>

constexpr std::uint8_t MODBUS_SOCKET = 0;
constexpr std::uint16_t MODBUS_PORT = 502;

static modbus_tcp_task_t modbus_task;

static Machine machine;
static modbus_context_t modbus_ctx = modbus_map_make_context(machine);

static ButtonPress start_button;
static LimitFilter limit_x_filter{3};
static LimitFilter limit_y_filter{3};

// The only place that knows pins and polarity.
static Inputs read_inputs(std::uint32_t now_ms) noexcept
{
    const bool limit_x_raw = (HAL_GPIO_ReadPin(X_HOME_GPIO_Port, X_HOME_Pin) == GPIO_PIN_SET);
    const bool limit_y_raw = (HAL_GPIO_ReadPin(Y_HOME_GPIO_Port, Y_HOME_Pin) == GPIO_PIN_SET);

    return Inputs{
        .e_stop_pressed = (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET),
        .start_pressed = (HAL_GPIO_ReadPin(START_GPIO_Port, START_Pin) == GPIO_PIN_RESET),
        .limit_x_pressed = limit_x_filter.update(limit_x_raw, now_ms),
        .limit_y_pressed = limit_y_filter.update(limit_y_raw, now_ms),
        .trigger_active =
            (HAL_GPIO_ReadPin(TRIGGER_GPIO_Port, TRIGGER_Pin) == GPIO_PIN_RESET),
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
    machine.loop_time_us =
        static_cast<std::uint16_t>(std::min<std::uint32_t>(elapsed_us, UINT16_MAX));
    machine.loop_time_max_us = std::max(machine.loop_time_max_us, machine.loop_time_us);
}

void setup()
{
    const uint32_t now = HAL_GetTick();

    ethernet_w5500_init(); // Initalize ethernet

    uint8_t version = getVERSIONR();
    std::printf("W5500 version register: 0x%02X (expect 0x04)\r\n", version);

    modbus_tcp_task_init(&modbus_task, MODBUS_SOCKET, MODBUS_PORT, &modbus_ctx);

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

    const uint32_t now = HAL_GetTick();

    machine.inputs = read_inputs(now);
    const Inputs &inputs = machine.inputs;

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

    modbus_tcp_task_run(&modbus_task);

    // Last, so the time covers the whole pass, Modbus included.
    record_loop_time(start_cycles);
}
