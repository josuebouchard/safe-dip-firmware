#include "entrypoint.hpp"
#include "hardware.hpp"
#include "stepper_motion_planner.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"

#include <cstdint>

// Each press of the blue user button (B1) turns the X axis 10 revolutions.

// 1.8 deg motor = 200 full steps/rev; the driver is set to 1600 pulses/rev (8x microstepping).
constexpr std::int32_t STEPS_PER_REVOLUTION = 1600;
constexpr std::int32_t TURNS_PER_PRESS = 10;
constexpr std::int32_t STEPS_PER_PRESS = STEPS_PER_REVOLUTION * TURNS_PER_PRESS;

// With 1000 steps/s^2 over 16000 steps the profile is triangular: it peaks
// at 4000 steps/s halfway and never reaches max_speed. The move takes ~8 s.
static StepperMotionPlanner::SpeedProfile speed_profile{
    .max_speed = 5000,
    .acceleration = 1000,
    .deceleration = 1000,
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile,
    hardware::stepper_driver_x,
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
