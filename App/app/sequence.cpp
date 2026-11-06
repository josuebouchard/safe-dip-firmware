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

// @todo: set from the measured axis travel and homing speed
constexpr std::uint32_t HOMING_TIMEOUT_MS = 10000;
// @todo: set from how far the switch hit varies on the bench. Must exceed the back-off,
// or an axis already resting on its switch at position 0 reads as drift.
constexpr std::int32_t HOMING_DRIFT_TOLERANCE_STEPS = 50;

// The only file that knows which operations exist. Function-local statics, so
// they are constructed when Machine calls this, not before its members.
Sequencer &make_sequence(Machine &machine)
{
    static HomeStepperOperation home_x{machine.controller_x, machine.inputs.limit_x_pressed, 20,
                                       HOMING_TIMEOUT_MS, HOMING_DRIFT_TOLERANCE_STEPS};
    static HomeStepperOperation home_y{machine.controller_y, machine.inputs.limit_y_pressed, 20,
                                       HOMING_TIMEOUT_MS, HOMING_DRIFT_TOLERANCE_STEPS};

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
