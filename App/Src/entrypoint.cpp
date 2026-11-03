#include "entrypoint.hpp"

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

void setup()
{
    const uint32_t now = HAL_GetTick();

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
        machine.sequencer.stop();
        machine.controller_x.stop_immediately();
        machine.controller_y.stop_immediately();
        hardware::effector.engage();
    }
    else
    {
        const uint32_t now = HAL_GetTick();

        if (inputs.start_pressed && !machine.sequencer.is_running())
        {
            // Reset and restart the sequencer
            machine.sequencer.restart();
        }

        // Sequencer step
        machine.sequencer.sequencer_step();

        // Process the different modules
        machine.controller_x.process(now);
        machine.controller_y.process(now);
    }

    modbus_tcp_task_run(&modbus_task);
}
