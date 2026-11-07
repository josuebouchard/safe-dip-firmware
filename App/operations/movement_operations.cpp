#include "movement_operations.hpp"
#include "stm32f4xx_hal.h"
#include <cstdlib>

void MoveStepperOperation::start() noexcept
{
    stepper_.move_to_steps(scale_.steps_from_mm(position_mm_));
}

Operation::PollResult MoveStepperOperation::poll() noexcept
{
    // Checked first: an interrupted move has its target moved to where it stopped.
    if (stepper_.was_interrupted())
    {
        return PollResult::aborted;
    }

    return stepper_.is_at_target() ? PollResult::finished : PollResult::running;
}

void HomeStepperOperation::start() noexcept
{
    is_backing_off_ = false;
    start_time_ = HAL_GetTick();
    failure_ = Failure::unspecified;
    stepper_.move_at_speed_in_direction(-1);
}

Operation::PollResult HomeStepperOperation::poll() noexcept
{
    if (!is_backing_off_)
    {
        if (is_switch_pressed_)
        {
            // The switch sits back_off_steps_ behind the origin, so a count far from
            // that means steps were lost since the last home.
            const std::int32_t drift = stepper_.position_current_signed() + back_off_steps_;

            if (has_origin_ && std::abs(drift) > drift_tolerance_steps_)
            {
                // The origin is wrong, so the next run sets a new one instead of failing again.
                has_origin_ = false;
                stepper_.stop_immediately();
                failure_ = homing_failure_;
                return PollResult::aborted;
            }

            stepper_.reset_position(-back_off_steps_);
            stepper_.move_to_steps(0);
            is_backing_off_ = true;
            has_origin_ = true;
        }
    }
    else if (stepper_.is_at_target())
    {
        if (is_switch_pressed_)
        {
            // Moved away and it still reads pressed, so it can't be trusted, and neither
            // can the origin it just set.
            has_origin_ = false;
            failure_ = homing_failure_;
            return PollResult::aborted;
        }

        return PollResult::finished;
    }

    if (HAL_GetTick() - start_time_ >= timeout_ms_)
    {
        // Stopping the sequencer doesn't stop the motors, so this axis would keep moving.
        stepper_.stop_immediately();
        failure_ = Failure::timed_out;
        return PollResult::aborted;
    }

    return PollResult::running;
}