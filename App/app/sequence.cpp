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
#include <tuple>

// How far past the switch hit the origin is set. Far enough to clear a microswitch's
// release travel. Per axis, so each can be tuned on the machine on its own.
constexpr std::int32_t HOMING_BACK_OFF_MM_X = 40;
constexpr std::int32_t HOMING_BACK_OFF_MM_Y = 20;

// The longest search covers the whole travel and the back-off at homing speed. Half as
// long again, plus a second for the ramps, so a slow but working axis doesn't time out.
static constexpr std::uint32_t homing_timeout_ms(const AxisScale &scale, std::int32_t travel_mm,
                                                 const StepperController::SpeedProfile &profile,
                                                 std::int32_t back_off_mm) noexcept
{
    const auto search_steps = static_cast<float>(scale.steps_from_mm(travel_mm + back_off_mm));
    return static_cast<std::uint32_t>(1500.0F * search_steps / profile.homing_speed) + 1000U;
}

// The only file that knows which operations exist. Function-local statics, so
// they are constructed when Machine calls this, not before its members.
Sequencer<Tick> &make_sequence(Machine &machine)
{
    static HomeStepperOperation home_x{
        machine.axis_x,
        Machine::SCALE_X.steps_from_mm(HOMING_BACK_OFF_MM_X),
        homing_timeout_ms(Machine::SCALE_X, Machine::X_TRAVEL_MM, machine.speed_profile_x,
                          HOMING_BACK_OFF_MM_X),
    };
    static HomeStepperOperation home_y{
        machine.axis_y,
        Machine::SCALE_Y.steps_from_mm(HOMING_BACK_OFF_MM_Y),
        homing_timeout_ms(Machine::SCALE_Y, Machine::Y_TRAVEL_MM, machine.speed_profile_y,
                          HOMING_BACK_OFF_MM_Y),
    };

    static MoveStepperOperation move_x_input{machine.axis_x, machine.recipe.x_input_mm};
    static MoveStepperOperation move_x_dip{machine.axis_x, machine.recipe.x_dip_mm};
    static MoveStepperOperation move_x_output{machine.axis_x, machine.recipe.x_output_mm};

    static MoveStepperOperation move_y_up{machine.axis_y, machine.recipe.y_up_mm};
    static MoveStepperOperation move_y_down{machine.axis_y, machine.recipe.y_down_mm};

    static EffectorOperation effector_engage{hardware::effector, true};
    static EffectorOperation effector_disengage{hardware::effector, false};

    static DwellOperation dwell{machine.recipe.dwell_ms};

    // The position in this array is the operation index on Modbus (input register 0).
    // docs/modbus-map.md and the STAGES list in tools/modbus_monitor/index.html copy
    // this order, so change them together. Each home operation takes the slot of the
    // move it replaces, so swapping one back doesn't shift the indices.
    static std::array<Operation<Tick> *, 13> operations = {
        // Home
        &home_y,
        // &move_y_up,
        &home_x,
        // &move_x_input,

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

    // advance_operation() wraps with `% size()`, so an empty cycle would divide by zero.
    static_assert(std::tuple_size_v<decltype(operations)> > 0);

    static Sequencer<Tick> sequencer{std::span<Operation<Tick> *>(operations)};

    return sequencer;
}
