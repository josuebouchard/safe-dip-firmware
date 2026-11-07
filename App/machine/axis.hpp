#ifndef AXIS_HPP
#define AXIS_HPP

#include "axis_scale.hpp"
#include "faults.hpp"
#include "inputs.hpp"
#include "stepper_controller.hpp"

// Everything an operation needs to drive one axis, bound once when the machine is
// built. Operations take one of these instead of reaching for an axis by name.
//
// Convention: the limit switch is at the negative end of the axis. Homing searches
// in the -1 direction, and loop()'s backstop stops any motion toward the switch.
//
// No default member values on purpose: every field must be given, and leaving one
// out of the designated initializer is then a -Wmissing-field-initializers warning.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct Axis
{
    StepperController &controller;
    AxisScale scale;

    // Which input is this axis's switch, read from the pass's snapshot as
    // `tick.inputs.*limit_pressed`.
    bool Inputs::*limit_pressed;

    Fault limit_hit;     // The switch was hit during a move toward it
    Fault homing_failed; // The switch misread while homing: stuck, or hit far from expected
};

#endif // AXIS_HPP
