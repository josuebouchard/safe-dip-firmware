#include "movement_operations.hpp"
#include <cstdlib>

void MoveStepperOperation::start(Tick & /*ctx*/) noexcept
{
    axis_.controller.move_to_steps(axis_.scale.steps_from_mm(position_mm_));
}

PollResult MoveStepperOperation::poll(Tick &ctx) noexcept
{
    StepperController &controller = axis_.controller;

    // Checked first: an interrupted move has its target moved to where it stopped.
    if (controller.was_interrupted())
    {
        ctx.faults.raise(Fault::operation_interrupted);
        return PollResult::aborted;
    }

    return controller.is_at_target() ? PollResult::finished : PollResult::running;
}

void HomeStepperOperation::start(Tick &ctx) noexcept
{
    is_backing_off_ = false;
    start_time_ = ctx.now_ms;
    axis_.controller.move_at_speed_in_direction(-1);
}

PollResult HomeStepperOperation::poll(Tick &ctx) noexcept
{
    StepperController &controller = axis_.controller;
    const bool is_switch_pressed = ctx.inputs.*axis_.limit_pressed;

    if (!is_backing_off_)
    {
        if (is_switch_pressed)
        {
            // The switch sits back_off_steps_ behind the origin, so a count far from
            // that means steps were lost since the last home.
            const std::int32_t drift = controller.position_current_signed() + back_off_steps_;

            if (has_origin_ && std::abs(drift) > drift_tolerance_steps_)
            {
                // The origin is wrong, so the next run sets a new one instead of failing again.
                has_origin_ = false;
                controller.stop_immediately();
                ctx.faults.raise(axis_.homing_failed);
                return PollResult::aborted;
            }

            controller.reset_position(-back_off_steps_);
            controller.move_to_steps(0);
            is_backing_off_ = true;
            has_origin_ = true;
        }
    }
    else if (controller.is_at_target())
    {
        if (is_switch_pressed)
        {
            // Moved away and it still reads pressed, so it can't be trusted, and neither
            // can the origin it just set.
            has_origin_ = false;
            ctx.faults.raise(axis_.homing_failed);
            return PollResult::aborted;
        }

        return PollResult::finished;
    }

    if (ctx.now_ms - start_time_ >= timeout_ms_)
    {
        // loop() stops both axes on this pass, once it sees the fault. Stopping this one
        // here too keeps the operation correct without relying on that.
        controller.stop_immediately();
        ctx.faults.raise(Fault::operation_timed_out);
        return PollResult::aborted;
    }

    return PollResult::running;
}
