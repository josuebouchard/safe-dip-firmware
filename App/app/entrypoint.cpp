#include "entrypoint.hpp"

#include "button_press.hpp"
#include "ethernet_if.h"
#include "hardware.hpp"
#include "inputs.hpp"
#include "machine.hpp"
#include "modbus_map.hpp"
#include "modbus_tcp_task.h"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "w5500.h"

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
        // (HAL_GPIO_ReadPin(Y_HOME_GPIO_Port, Y_HOME_Pin) == GPIO_PIN_SET)
        .limit_y_pressed = false,
    };
}

static Fault to_fault(Failure failure) noexcept
{
    switch (failure)
    {
    case Failure::interrupted:
        return Fault::operation_interrupted;
    case Failure::timed_out:
        return Fault::operation_timed_out;
    case Failure::x_homing_failed:
        return Fault::x_homing_failed;
    case Failure::y_homing_failed:
        return Fault::y_homing_failed;
    case Failure::unspecified:
        break;
    }

    return Fault::operation_failed;
}

static void enter_safe_state() noexcept
{
    machine.sequencer.stop();
    machine.controller_x.stop_immediately();
    machine.controller_y.stop_immediately();
    hardware::effector.engage();
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
}

void loop()
{
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

    if (machine.faults.any())
    {
        enter_safe_state();
    }
    else
    {
        Tick tick{.now_ms = now, .inputs = machine.inputs, .faults = machine.faults};
        const auto failure = machine.sequencer.step(tick);
        if (failure.has_value())
        {
            machine.faults.raise(to_fault(*failure));
        }

        machine.controller_x.process(now);
        machine.controller_y.process(now);
    }

    modbus_tcp_task_run(&modbus_task);
}
