#include "movement_operations.hpp"
#include "stm32f4xx_hal.h"

void MoveStepperOperation::start() noexcept { stepper_.move_to_steps(position_); }

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
            stepper_.reset_position(-back_off_steps_);
            stepper_.move_to_steps(0);
            is_backing_off_ = true;
        }
    }
    else if (stepper_.is_at_target())
    {
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
