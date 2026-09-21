#include "entrypoint.hpp"
#include "hardware.hpp"
#include "stepper_motion_planner.hpp"

#include "stm32f4xx_hal.h"

static StepperMotionPlanner::SpeedProfile speed_profile{
    .max_speed = 700,
    .acceleration = 200,
    .deceleration = 200,
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile,
    hardware::stepper_driver_x,
};

void setup()
{
    stepper_movement_x.initialize(HAL_GetTick());

    // Swap these to switch modes: move_at_speed() holds a speed
    // indefinitely with no position target, move_to_steps() ramps to a
    // target and stops there.
    // stepper_movement_x.move_at_speed(100.0f);
    stepper_movement_x.move_to_steps(300);
}

void loop() { stepper_movement_x.process(HAL_GetTick()); }
