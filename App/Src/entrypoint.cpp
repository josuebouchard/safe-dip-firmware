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

#include "stm32f446xx.h"
#include "stm32f4xx_hal.h"
#include "timer_operations.hpp"
#include "w5500.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

constexpr std::uint8_t ECHO_SOCKET = 0;
constexpr std::uint16_t ECHO_PORT = 502;

extern TIM_HandleTypeDef htim2;

static modbus_tcp_task_t modbus_task;
static modbus_context_t modbus_ctx{};

static STM32OutputCompareChannel OC_timer_channel_x{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = TIM_CHANNEL_1,
    },
};
// @todo: placeholder pin pending real hardware pin-out.
static StepperDriver stepper_driver_x{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_x,
        .direction =
            StepperDriver::DirectionPin{
                .port = GPIOA,
                .pin = GPIO_PIN_12,
                .electrical_state_clockwise = GPIO_PIN_SET,
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
    },
};

// @todo: placeholder pin pending real hardware pin-out (not PA13 - that's SWDIO/debug).
static StepperDriver stepper_driver_y{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_y,
        .direction =
            StepperDriver::DirectionPin{
                .port = GPIOA,
                .pin = GPIO_PIN_15,
                .electrical_state_clockwise = GPIO_PIN_SET,

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

// @todo: placeholder pin pending real hardware pin-out.
static EffectorDriver effector{
    GPIOA,
    GPIO_PIN_11,
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

    stepper_movement_x.initialize(now);
    stepper_movement_y.initialize(now);

    sequencer.start();

    ethernet_W5500_init();

    uint8_t version = getVERSIONR();
    std::printf("W5500 version register: 0x%02X (expect 0x04)\r\n", version);

    modbus_tcp_task_init(&modbus_task, ECHO_SOCKET, ECHO_PORT, &modbus_ctx);
}

void loop()
{
    const uint32_t now = HAL_GetTick();

    modbus_tcp_task_run(&modbus_task);

    sequencer.sequencer_step();

    stepper_movement_x.process(now);
    stepper_movement_y.process(now);
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
