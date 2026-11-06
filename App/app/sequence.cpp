#include "sequence.hpp"

#include "effector_operations.hpp"
#include "hardware.hpp"
#include "machine.hpp"
#include "movement_operations.hpp"
#include "operation.hpp"
#include "timer_operations.hpp"

#include <array>
#include <cstdint>
#include <span>

// How far past the switch hit the origin is set. Far enough to clear a microswitch's
// release travel, which is usually under 1 mm. Too short reads as a stuck switch.
constexpr std::int32_t HOMING_BACK_OFF_MM = 2;

// How far from the expected spot a switch hit can be before it counts as lost steps.
// It covers the switch's repeatability and how far the axis coasts before loop() sees
// the switch, both well under 1 mm. It must exceed the back-off: a switch that only
// just released at the end of the last back-off can read pressed again at position 0,
// which is a back-off away from the expected hit.
constexpr std::int32_t HOMING_DRIFT_TOLERANCE_MM = 3;
static_assert(HOMING_DRIFT_TOLERANCE_MM > HOMING_BACK_OFF_MM);

// The longest search covers the whole travel and the back-off at homing speed. Half as
// long again, plus a second for the ramps, so a slow but working axis doesn't time out.
static std::uint32_t homing_timeout_ms(const AxisScale &scale, std::int32_t travel_mm,
                                       const StepperController::SpeedProfile &profile) noexcept
{
    const auto search_steps =
        static_cast<float>(scale.steps_from_mm(travel_mm + HOMING_BACK_OFF_MM));
    return static_cast<std::uint32_t>(1500.0F * search_steps / profile.homing_speed) + 1000U;
}

// The only file that knows which operations exist. Function-local statics, so
// they are constructed when Machine calls this, not before its members.
Sequencer &make_sequence(Machine &machine)
{
    static HomeStepperOperation home_x{
        machine.controller_x,
        machine.inputs.limit_x_pressed,
        Machine::SCALE_X.steps_from_mm(HOMING_BACK_OFF_MM),
        homing_timeout_ms(Machine::SCALE_X, Machine::X_TRAVEL_MM, machine.speed_profile_x),
        Machine::SCALE_X.steps_from_mm(HOMING_DRIFT_TOLERANCE_MM),
    };
    static HomeStepperOperation home_y{
        machine.controller_y,
        machine.inputs.limit_y_pressed,
        Machine::SCALE_Y.steps_from_mm(HOMING_BACK_OFF_MM),
        homing_timeout_ms(Machine::SCALE_Y, Machine::Y_TRAVEL_MM, machine.speed_profile_y),
        Machine::SCALE_Y.steps_from_mm(HOMING_DRIFT_TOLERANCE_MM),
    };

    static MoveStepperOperation move_x_input{machine.controller_x, Machine::SCALE_X,
                                             machine.recipe.x_input_mm};
    static MoveStepperOperation move_x_dip{machine.controller_x, Machine::SCALE_X,
                                           machine.recipe.x_dip_mm};
    static MoveStepperOperation move_x_output{machine.controller_x, Machine::SCALE_X,
                                              machine.recipe.x_output_mm};

    static MoveStepperOperation move_y_up{machine.controller_y, Machine::SCALE_Y,
                                          machine.recipe.y_up_mm};
    static MoveStepperOperation move_y_down{machine.controller_y, Machine::SCALE_Y,
                                            machine.recipe.y_down_mm};

    static EffectorOperation effector_engage{hardware::effector, true};
    static EffectorOperation effector_disengage{hardware::effector, false};

    static DwellOperation dwell{machine.recipe.dwell_ms};

    static std::array<Operation *, 13> operations = {
        // Home
        // &home_y,
        // &home_x,
        &move_y_up,
        &move_x_input,

        // Pick
        &move_y_down,
        &effector_engage,
        &move_y_up,

        // Dip
        &move_x_dip,
        &move_y_down,
        &dwell,
        &move_y_up,

        // Deposit
        &move_x_output,
        &move_y_down,

        // Release
        &effector_disengage,
        &move_y_up,
    };

    static Sequencer sequencer{std::span<Operation *>(operations)};

    return sequencer;
}
