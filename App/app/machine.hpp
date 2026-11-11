#ifndef MACHINE_HPP
#define MACHINE_HPP

#include "axis.hpp"
#include "axis_scale.hpp"
#include "faults.hpp"
#include "hardware.hpp"
#include "inputs.hpp"
#include "recipe.hpp"
#include "sequence.hpp"
#include "sequencer.hpp"
#include "stepper_controller.hpp"
#include "tick.hpp"

#include <cstdint>

// The application state, owned by loop(). `sequencer` must stay the last member:
// building it binds operations to the members above.
struct Machine
{
    // Set on both drivers' DIP switches: 200 full steps x 8 microsteps.
    static constexpr std::int32_t PULSES_PER_REV = 1600;

    // X: 2GT belt (2 mm pitch) on a 20-tooth pulley, 40 mm per rev.
    static constexpr AxisScale SCALE_X = AxisScale::pulley(PULSES_PER_REV, 20, 2);
    // Y: 8 mm acme leadscrew, 2 mm pitch x 4 starts = 8 mm lead.
    static constexpr AxisScale SCALE_Y = AxisScale::leadscrew(PULSES_PER_REV, 8);

    // Usable travel from the origin set by homing, in mm. Y can't exceed the 300 mm
    // screw, less the nut and the end supports.
    // @todo: set from the measured axis travel
    static constexpr std::int32_t X_TRAVEL_MM = 400;
    static constexpr std::int32_t Y_TRAVEL_MM = 200;

    Inputs inputs;
    Recipe recipe;
    Faults faults;

    // Time of one whole loop pass, Modbus included, in µs, saturated at 65535.
    std::uint16_t loop_time_us = 0;
    std::uint16_t loop_time_max_us = 0; // Since reset

    // In mm/s and mm/s^2
    StepperController::SpeedProfile speed_profile_x{
        SCALE_X.steps_per_mm() * 200.0F, // max_speed
        SCALE_X.steps_per_mm() * 100.0F, // acceleration
        SCALE_X.steps_per_mm() * 2.5F,   // landing_speed
        SCALE_X.steps_per_mm() * 50.0F,  // homing_speed
    };
    StepperController::SpeedProfile speed_profile_y{
        SCALE_Y.steps_per_mm() * 50.0F, // max_speed
        SCALE_Y.steps_per_mm() * 40.0F, // acceleration
        SCALE_Y.steps_per_mm() * 0.5F,  // landing_speed
        SCALE_Y.steps_per_mm() * 10.0F, // homing_speed
    };

    StepperController controller_x{speed_profile_x, hardware::stepper_driver_x};
    StepperController controller_y{speed_profile_y, hardware::stepper_driver_y};

    // What operations get to drive each axis.
    Axis axis_x{
        .controller = controller_x,
        .scale = SCALE_X,
        .limit_pressed = &Inputs::limit_x_pressed,
        .limit_hit = Fault::x_limit_hit,
        .homing_failed = Fault::x_homing_failed,
    };
    Axis axis_y{
        .controller = controller_y,
        .scale = SCALE_Y,
        .limit_pressed = &Inputs::limit_y_pressed,
        .limit_hit = Fault::y_limit_hit,
        .homing_failed = Fault::y_homing_failed,
    };

    Sequencer<Tick> &sequencer = make_sequence(*this);
};

#endif // MACHINE_HPP
