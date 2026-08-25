#include "entrypoint.hpp"
#include "stepper_driver.hpp"
#include "stepper_motion_planner.hpp"
#include "stm32_output_compare_channel.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

extern TIM_HandleTypeDef htim2;

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
                .port = DIR_x_GPIO_Port,
                .pin = DIR_x_Pin,
                .electrical_state_clockwise = GPIO_PIN_SET,
            },
        .timer_ticks_per_second = 1000000,
    },
};

// static StepperMotionPlanner::SpeedProfile speed_profile{
//     .max_speed = 1000,
//     .acceleration = 200,
//     .deceleration = 200,
// };

// static StepperMotionPlanner stepper_movement_x{
//     speed_profile,
//     stepper_driver_x,
// };

void setup()
{
    // stepper_movement_x.initialize(HAL_GetTick());
    // stepper_movement_x.move_continously(1);
    // HAL_TIM_OC_Start_IT(&htim2, 1);
    // set_speed() now takes steps/second (was steps/ms) - 1000 steps/sec reproduces
    // the same 1kHz test signal (half_period_ticks_=500) verified working earlier.
    stepper_driver_x.set_speed(1000);
}

void loop() {
    // stepper_movement_x.process(HAL_GetTick());
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
