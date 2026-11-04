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
#include "stm32f4xx_hal_gpio.h"
#include "w5500.h"

#include <cstdint>
#include <cstdio>
#include <stdbool.h>

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
        // (HAL_GPIO_ReadPin(X_HOME_GPIO_Port, X_HOME_Pin) == GPIO_PIN_SET)
        .limit_x_pressed = false,
        // (HAL_GPIO_ReadPin(Y_HOME_GPIO_Port, Y_HOME_Pin) == GPIO_PIN_SET)
        .limit_y_pressed = false,
    };
}

// Reads and clears the reset flags, which otherwise pile up across resets. A power-on
// or the reset button is expected. The pin flag doesn't tell them apart from the rest,
// because every reset drives the pin, and a power-on also sets the brownout flag.
static void check_reset_cause() noexcept
{
    const std::uint32_t flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    const bool is_brownout = (flags & RCC_CSR_BORRSTF) && !(flags & RCC_CSR_PORRSTF);
    const bool is_internal =
        flags & (RCC_CSR_SFTRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF);

    if (is_brownout || is_internal)
        machine.faults.raise(Fault::unexpected_reset, static_cast<std::uint16_t>(flags >> 24));
}

static Fault to_fault(Operation::Failure failure) noexcept
{
    switch (failure)
    {
    case Operation::Failure::interrupted:
        return Fault::operation_interrupted;
    case Operation::Failure::timed_out:
        return Fault::operation_timed_out;
    case Operation::Failure::sensor_stuck:
        return Fault::home_switch_stuck;
    case Operation::Failure::out_of_tolerance:
        return Fault::position_drift;
    case Operation::Failure::unspecified:
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

    check_reset_cause();

    // Initalize ethernet

    ethernet_W5500_init();

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

    if (machine.faults.any())
    {
        // Only clears: a separate press starts the machine, so one press can't do both.
        if (start_press && !inputs.e_stop_pressed)
        {
            machine.faults.clear();
        }
    }
    else
    {
        if (start_press && !machine.sequencer.is_running())
        {
            machine.sequencer.restart(); // Reset and restart the sequencer

            // Re-initialize the controllers to now
            machine.controller_x.initialize(now);
            machine.controller_y.initialize(now);
        }

        // Sequencer step
        const auto failure = machine.sequencer.sequencer_step();
        if (failure)
        {
            machine.faults.raise(to_fault(*failure),
                                 static_cast<std::uint16_t>(machine.sequencer.operation_index()));
        }
    }

    if (machine.faults.any())
    {
        enter_safe_state();
    }
    else
    {
        // Process the different modules
        machine.controller_x.process(now);
        machine.controller_y.process(now);
    }

    modbus_tcp_task_run(&modbus_task);
}
