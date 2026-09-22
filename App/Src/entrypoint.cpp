#include "entrypoint.hpp"

#include "effector_operations.hpp"
#include "ethernet_if.h"
#include "hardware.hpp"
#include "modbus_tcp_task.h"
#include "movement_operations.hpp"
#include "operation.hpp"
#include "sequencer.hpp"
#include "stepper_motion_planner.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "timer_operations.hpp"
#include "w5500.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdbool.h>

constexpr std::uint8_t ECHO_SOCKET = 0;
constexpr std::uint16_t ECHO_PORT = 502;

static modbus_tcp_task_t modbus_task;
static modbus_context_t modbus_ctx{};

// Named rather than a temporary: StepperMotionPlanner stores the profile by
// reference, so a temporary would not bind and would dangle if it did.
static StepperMotionPlanner::SpeedProfile speed_profile_x{
    1000, // max_speed
    200,  // acceleration
    50,   // landing_speed
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile_x,
    hardware::stepper_driver_x,
};

static StepperMotionPlanner::SpeedProfile speed_profile_y{
    1000, // max_speed
    200,  // acceleration
    50,   // landing_speed
};

static StepperMotionPlanner stepper_movement_y{
    speed_profile_y,
    hardware::stepper_driver_y,
};

static std::uint32_t dwell_time_ms = 200;

// === Operations ===
static MoveStepperOperation move_x_input{stepper_movement_x, 0};
static MoveStepperOperation move_x_dip{stepper_movement_x, 500};
static MoveStepperOperation move_x_output{stepper_movement_x, 1000};

static MoveStepperOperation move_y_up{stepper_movement_y, 0};
static MoveStepperOperation move_y_down{stepper_movement_y, 500};

static EffectorEngageOperation effector_engage{hardware::effector};
static EffectorDisengageOperation effector_disengage{hardware::effector};

static DwellOperation dwell_operation{dwell_time_ms};

static std::array<Operation *, 2> operations = {
    &move_x_input,
    &move_x_output,
};

static Sequencer sequencer{std::span<Operation *>(operations)};

void setup()
{
    const uint32_t now = HAL_GetTick();

    // Initalize ethernet

    ethernet_W5500_init();

    uint8_t version = getVERSIONR();
    std::printf("W5500 version register: 0x%02X (expect 0x04)\r\n", version);

    modbus_tcp_task_init(&modbus_task, ECHO_SOCKET, ECHO_PORT, &modbus_ctx);

    // Initialize movement planners
    stepper_movement_x.initialize(now);
    stepper_movement_y.initialize(now);
}

void loop()
{
    // @todo: maybe convert to external interrupt
    // Active low: the button pulls the pin to ground against its internal pull-up.
    bool is_e_stop_pressed = (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) != GPIO_PIN_SET);
    if (is_e_stop_pressed)
    {
        sequencer.stop();
        stepper_movement_x.stop_immediately();
        stepper_movement_y.stop_immediately();
        hardware::effector.engage();
    }
    else // Do work
    {
        const uint32_t now = HAL_GetTick();

        // Start button process
        bool is_start_button_pressed = (HAL_GPIO_ReadPin(START_GPIO_Port, START_Pin) != GPIO_PIN_SET);
        if (!sequencer.is_running() && is_start_button_pressed)
        {
            // Start sequencer
            sequencer.start();
        }

        // Sequencer step
        sequencer.sequencer_step();

        // Process the different modules
        stepper_movement_x.process(now);
        stepper_movement_y.process(now);
    }

    modbus_tcp_task_run(&modbus_task);
}
