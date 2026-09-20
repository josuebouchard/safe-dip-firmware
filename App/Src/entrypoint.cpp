#include "entrypoint.hpp"

#include "effector_driver.hpp"
#include "effector_operations.hpp"
#include "ethernet_if.h"
#include "modbus_tcp_task.h"
#include "movement_operations.hpp"
#include "operation.hpp"
#include "sequencer.hpp"
#include "stepper_driver.hpp"
#include "stepper_motion_planner.hpp"
#include "stm32_output_compare_channel.hpp"

#include "main.h"
#include "stm32f446xx.h"
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

extern TIM_HandleTypeDef htim2;

static modbus_tcp_task_t modbus_task;
static modbus_context_t modbus_ctx{};

static STM32OutputCompareChannel OC_timer_channel_x{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = TIM_CHANNEL_1,
        .polarity = TIM_OCPOLARITY_LOW,
    },
};
static StepperDriver stepper_driver_x{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_x,
        .direction =
            StepperDriver::DirectionPin{
                .port = DIR_x_GPIO_Port,
                .pin = DIR_x_Pin,
                // Sinking output: the driver sees current when the pin is low.
                .electrical_state_clockwise = GPIO_PIN_RESET,
            },
        .timer_ticks_per_second = 1000000,
    },
};

// Named rather than a temporary: StepperMotionPlanner stores the profile by
// reference, so a temporary would not bind and would dangle if it did.
static StepperMotionPlanner::SpeedProfile speed_profile_x{
    .max_speed = 1000,
    .acceleration = 200,
    .deceleration = 200,
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile_x,
    stepper_driver_x,
};

static STM32OutputCompareChannel OC_timer_channel_y{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = TIM_CHANNEL_2,
        .polarity = TIM_OCPOLARITY_LOW,
    },
};

static StepperDriver stepper_driver_y{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_y,
        .direction =
            StepperDriver::DirectionPin{
                .port = DIR_y_GPIO_Port,
                .pin = DIR_y_Pin,
                // Sinking output: the driver sees current when the pin is low.
                .electrical_state_clockwise = GPIO_PIN_RESET,
            },
        .timer_ticks_per_second = 1000000,
    },
};

static StepperMotionPlanner::SpeedProfile speed_profile_y{
    .max_speed = 1000,
    .acceleration = 200,
    .deceleration = 200,
};

static StepperMotionPlanner stepper_movement_y{
    speed_profile_y,
    stepper_driver_y,
};

static EffectorDriver effector{
    EFFECTOR_GPIO_Port,
    EFFECTOR_Pin,
};

static std::uint32_t dwell_time_ms = 200;

// === Operations ===
static MoveStepperOperation move_x_input{stepper_movement_x, 0};
static MoveStepperOperation move_x_dip{stepper_movement_x, 500};
static MoveStepperOperation move_x_output{stepper_movement_x, 1000};

static MoveStepperOperation move_y_up{stepper_movement_y, 0};
static MoveStepperOperation move_y_down{stepper_movement_y, 500};

static EffectorEngageOperation effector_engage{effector};
static EffectorDisengageOperation effector_disengage{effector};

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
        effector.engage();
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

extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2)
    {
        return;
    }

    switch (htim->Channel)
    {
    case HAL_TIM_ACTIVE_CHANNEL_1:
        stepper_driver_x.on_compare_elapsed();
        break;
    case HAL_TIM_ACTIVE_CHANNEL_2:
        stepper_driver_y.on_compare_elapsed();
        break;
    default:
        break;
    }
}
