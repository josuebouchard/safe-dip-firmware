#ifndef MACHINE_HPP
#define MACHINE_HPP

#include "hardware.hpp"
#include "inputs.hpp"
#include "recipe.hpp"
#include "sequence.hpp"
#include "sequencer.hpp"
#include "stepper_motion_planner.hpp"

// The application state, owned by loop(). `sequencer` must stay the last member:
// building it binds operations to the members above.
struct Machine
{
    Inputs inputs;
    Recipe recipe;

    StepperMotionPlanner::SpeedProfile speed_profile_x{
        8000, // max_speed
        4000, // acceleration
        100,  // landing_speed
        2000, // homing_speed
    };
    StepperMotionPlanner::SpeedProfile speed_profile_y{
        10000, // max_speed
        8000,  // acceleration
        100,   // landing_speed
        2000,  // homing_speed
    };

    StepperMotionPlanner planner_x{speed_profile_x, hardware::stepper_driver_x};
    StepperMotionPlanner planner_y{speed_profile_y, hardware::stepper_driver_y};

    Sequencer &sequencer = make_sequence(*this);
};

#endif // MACHINE_HPP
