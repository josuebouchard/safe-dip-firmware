#include "entrypoint.hpp"
#include "hardware.hpp"
#include "stepper_motion_planner.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"

#include <cstdint>

// 1.8 deg motor = 200 full steps/rev; the driver is set to 1600 pulses/rev (8x microstepping).
constexpr std::int32_t STEPS_PER_REVOLUTION = 1600;
constexpr std::int32_t TURNS_PER_PRESS = 10;
constexpr std::int32_t STEPS_PER_PRESS = STEPS_PER_REVOLUTION * TURNS_PER_PRESS;

constexpr float ACCELERATION = 4000.0f;  // steps/s^2
constexpr float CRUISE_SPEED = 4000.0f;  // steps/s
constexpr float LANDING_SPEED = 100.0f;  // steps/s

// The ramps must fit in the move, or it never reaches cruise speed.
static_assert(CRUISE_SPEED * CRUISE_SPEED / ACCELERATION <= STEPS_PER_PRESS);
static_assert(STEPS_PER_PRESS / CRUISE_SPEED + CRUISE_SPEED / ACCELERATION == 5.0f);

static StepperMotionPlanner::SpeedProfile speed_profile{
    CRUISE_SPEED,  // max_speed
    ACCELERATION,  // acceleration
    LANDING_SPEED, // landing_speed
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile,
    hardware::stepper_driver_x,
};

static bool was_button_down = false;

void setup() { stepper_movement_x.initialize(HAL_GetTick()); }

void loop()
{
    // Edge-triggered so holding the button doesn't re-trigger once the move finishes.
    const bool is_button_down = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
    const bool was_pressed = is_button_down && !was_button_down;
    was_button_down = is_button_down;

    // Ignoring presses mid-move also swallows contact bounce.
    if (was_pressed && stepper_movement_x.is_at_target())
    {
        stepper_movement_x.move_to_steps(stepper_movement_x.position_current_signed() +
                                         STEPS_PER_PRESS);
    }

    stepper_movement_x.process(HAL_GetTick());
}
