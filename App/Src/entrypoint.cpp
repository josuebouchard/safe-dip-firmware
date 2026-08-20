#include "entrypoint.hpp"

#include "effector_driver.hpp"
#include "effector_module.hpp"
#include "movement_module.hpp"
#include "operation.hpp"
#include "sequencer.hpp"
#include "stepper_driver.hpp"
#include "stepper_motion_planner.hpp"
#include "stm32_output_compare_channel.hpp"

#include "stm32f446xx.h"
#include "stm32f4xx_hal.h"
#include "timer_operations.hpp"

#include <array>
#include <cstdint>
#include <span>

constexpr std::uint32_t ECHO_SOCKET = 0;
constexpr std::uint32_t ECHO_PORT = 502;

extern TIM_HandleTypeDef htim2;

static STM32OutputCompareChannel OC_timer_channel_x{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = 1,
    },
};
static StepperDriver stepper_driver_x{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_x,
        .direction =
            StepperDriver::DirectionPin{
                .port = GPIOA,
                .pin = 12,
                .electrical_state_clockwise = GPIO_PIN_SET,
            },
        .timer_ticks_per_ms = 1000,
    },
};

static StepperMotionPlanner stepper_movement_x{
    StepperMotionPlanner::SpeedProfile{
        .max_speed = 1000,
        .acceleration = 200,
        .deceleration = 200,
    },
    stepper_driver_x,
};

static STM32OutputCompareChannel OC_timer_channel_y{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = 2,
    },
};

static StepperDriver stepper_driver_y{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_y,
        .direction =
            StepperDriver::DirectionPin{
                .port = GPIOA,
                .pin = 13,
                .electrical_state_clockwise = GPIO_PIN_SET,

            },
        .timer_ticks_per_ms = 1000,
    },
};

static StepperMotionPlanner stepper_movement_y{
    StepperMotionPlanner::SpeedProfile{
        .max_speed = 1000,
        .acceleration = 200,
        .deceleration = 200,
    },
    stepper_driver_y,
};

static EffectorDriver effector{
    GPIOA,
    23,
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

std::array<Operation *, 2> operations = {
    &move_x_input,
    &move_x_output,
};

static Sequencer sequencer{std::span<Operation *>(operations)};

void setup()
{
    const uint32_t now = HAL_GetTick();

    stepper_movement_x.initialize(now);
    stepper_movement_y.initialize(now);
}

void loop()
{
    const uint32_t now = HAL_GetTick();

    sequencer.sequencer_step();

    stepper_movement_x.process(now);
    stepper_movement_y.process(now);
}