#include "entrypoint.hpp"
#include "stepper_driver.hpp"
#include "stepper_motion_planner.hpp"
#include "stm32_output_compare_channel.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

#include <cstdint>

// Each press of the blue user button (B1) turns the X axis 10 revolutions.

// 1.8 deg motor = 200 full steps/rev; the driver is set to 1600 pulses/rev (8x microstepping).
constexpr std::int32_t STEPS_PER_REVOLUTION = 1600;
constexpr std::int32_t TURNS_PER_PRESS = 10;
constexpr std::int32_t STEPS_PER_PRESS = STEPS_PER_REVOLUTION * TURNS_PER_PRESS;

extern TIM_HandleTypeDef htim2;

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

// With 1000 steps/s^2 over 16000 steps the profile is triangular: it peaks
// at 4000 steps/s halfway and never reaches max_speed. The move takes ~8 s.
static StepperMotionPlanner::SpeedProfile speed_profile{
    .max_speed = 5000,
    .acceleration = 1000,
    .deceleration = 1000,
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile,
    stepper_driver_x,
};

// B1 is active low: the board pulls it up and the button shorts it to ground.
static bool was_button_down = false;

void setup() { stepper_movement_x.initialize(HAL_GetTick()); }

void loop()
{
    // Act on the press edge, not the level - holding the button must not
    // re-trigger once the move finishes.
    const bool is_button_down = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
    const bool was_pressed = is_button_down && !was_button_down;
    was_button_down = is_button_down;

    // Presses during a move are ignored rather than queued; this also
    // swallows contact bounce, since the move outlasts it by seconds.
    if (was_pressed && stepper_movement_x.is_at_target())
    {
        stepper_movement_x.move_to_steps(stepper_movement_x.position_current_signed() +
                                         STEPS_PER_PRESS);
    }

    stepper_movement_x.process(HAL_GetTick());
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
    default:
        break;
    }
}
