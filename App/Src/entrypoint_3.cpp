#include "entrypoint.hpp"
#include "hardware.hpp"
#include "stepper_motion_planner.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"

#include <cstdint>

// Each press of the blue user button (B1) turns the X axis 10 revolutions in 5 seconds.

// 1.8 deg motor = 200 full steps/rev; the driver is set to 1600 pulses/rev (8x microstepping).
constexpr std::int32_t STEPS_PER_REVOLUTION = 1600;
constexpr std::int32_t TURNS_PER_PRESS = 10;
constexpr std::int32_t STEPS_PER_PRESS = STEPS_PER_REVOLUTION * TURNS_PER_PRESS;

// Trapezoidal move: ramp up for 1 s (2000 steps), cruise at 4000 steps/s
// for 3 s (12000 steps), ramp down for 1 s (2000 steps).
constexpr float ACCELERATION = 4000.0f;  // steps/s^2
constexpr float CRUISE_SPEED = 4000.0f;  // steps/s
constexpr float LANDING_SPEED = 100.0f;  // steps/s

// A symmetric trapezoid lasts distance/v + v/a, provided the ramps fit in
// the move (v^2/a <= distance) - otherwise it never reaches cruise speed.
static_assert(CRUISE_SPEED * CRUISE_SPEED / ACCELERATION <= STEPS_PER_PRESS);
static_assert(STEPS_PER_PRESS / CRUISE_SPEED + CRUISE_SPEED / ACCELERATION == 5.0f);

static StepperMotionPlanner::SpeedProfile speed_profile{
    CRUISE_SPEED,  // max_speed
    ACCELERATION,  // acceleration
    ACCELERATION,  // deceleration
    LANDING_SPEED, // landing_speed
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
