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
    Machine() = default;

    // The controllers, axes and operations hold references to this object's own
    // members, so a copy would still drive the original's.
    Machine(const Machine &) = delete;
    Machine &operator=(const Machine &) = delete;

    // Set on both drivers' DIP switches: 200 full steps x 8 microsteps.
    static constexpr std::int32_t PULSES_PER_REV = 1600;

    // X: 2GT belt (2 mm pitch) on a 20-tooth pulley, 40 mm per rev.
    static constexpr AxisScale SCALE_X = AxisScale::pulley(PULSES_PER_REV, 20, 2);
    // Y: 8 mm acme leadscrew (Tr8x2), 2 mm pitch x 1 start = 2 mm lead.
    static constexpr AxisScale SCALE_Y = AxisScale::leadscrew(PULSES_PER_REV, 2);

    // Physical stroke in mm, measured from the surface nearest the limit switch to the far end,
    // ignoring the switch's own size. Y can't exceed the 300 mm screw, less the nut and the end
    // supports.
    static constexpr std::int32_t X_PHYSICAL_MM = 590;
    static constexpr std::int32_t Y_PHYSICAL_MM = 220;

    // How far past the switch hit the origin is set. Far enough to clear a microswitch's
    // release travel. Moving it moves the origin, so every recipe value shifts with it.
    static constexpr std::int32_t X_HOMING_BACK_OFF_MM = 40;
    static constexpr std::int32_t Y_HOMING_BACK_OFF_MM = 20;

    // Usable travel from the origin set by homing, in mm.
    static constexpr std::int32_t X_TRAVEL_MM = X_PHYSICAL_MM - X_HOMING_BACK_OFF_MM;
    static constexpr std::int32_t Y_TRAVEL_MM = Y_PHYSICAL_MM - Y_HOMING_BACK_OFF_MM;

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
        SCALE_Y.steps_per_mm() * 20.0F, // max_speed
        SCALE_Y.steps_per_mm() * 10.0F, // acceleration
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
