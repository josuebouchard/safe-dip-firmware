#include "movement_operations.hpp"

void MoveStepperOperation::start() noexcept { stepper_.move_to_steps(position_); }

Operation::PollResult MoveStepperOperation::poll() noexcept
{
    return stepper_.is_at_target() ? PollResult::finished : PollResult::running;
}

void HomeStepperOperation::start() noexcept
{
    is_backing_off_ = false;
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

        return PollResult::running;
    }
    else
    {
        if (!stepper_.is_at_target())
        {
            return PollResult::running;
        }
        else
        {
            return PollResult::finished;
        }
    }
}