#include "hardware.hpp"
#include "stm32_output_compare_channel.hpp"

#include "main.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_tim.h"

#include <cstdint>

extern TIM_HandleTypeDef htim2;

// TIM2 counts at 84 MHz / (prescaler 83 + 1) = 1 MHz - see nucleo-project.ioc.
constexpr std::uint32_t TIM2_TICKS_PER_SECOND = 1000000;

// The channels must be defined before the drivers that reference them: both
// live in this translation unit, so definition order is initialization order.
static STM32OutputCompareChannel OC_timer_channel_x{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = TIM_CHANNEL_1,
        // Sinking output: the active compare level pulls the pin low.
        .polarity = TIM_OCPOLARITY_LOW,
    },
};

static STM32OutputCompareChannel OC_timer_channel_y{
    STM32OutputCompareChannel::Config{
        .timer = &htim2,
        .timer_channel = TIM_CHANNEL_2,
        .polarity = TIM_OCPOLARITY_LOW,
    },
};

namespace hardware
{

StepperDriver stepper_driver_x{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_x,
        .direction =
            StepperDriver::DirectionPin{
                .port = DIR_x_GPIO_Port,
                .pin = DIR_x_Pin,
                // Sinking output: the driver sees current when the pin is low.
                .electrical_state_clockwise = GPIO_PIN_RESET,
            },
        .timer_ticks_per_second = TIM2_TICKS_PER_SECOND,
    },
};

StepperDriver stepper_driver_y{
    StepperDriver::Config{
        .timer_channel = OC_timer_channel_y,
        .direction =
            StepperDriver::DirectionPin{
                .port = DIR_y_GPIO_Port,
                .pin = DIR_y_Pin,
                .electrical_state_clockwise = GPIO_PIN_SET,
            },
        .timer_ticks_per_second = TIM2_TICKS_PER_SECOND,
    },
};

EffectorDriver effector{
    EFFECTOR_GPIO_Port,
    EFFECTOR_Pin,
};

} // namespace hardware

extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2)
    {
        return;
    }

    switch (htim->Channel)
    {
    case HAL_TIM_ACTIVE_CHANNEL_1:
        hardware::stepper_driver_x.on_compare_elapsed();
        break;
    case HAL_TIM_ACTIVE_CHANNEL_2:
        hardware::stepper_driver_y.on_compare_elapsed();
        break;
    default:
        break;
    }
}
