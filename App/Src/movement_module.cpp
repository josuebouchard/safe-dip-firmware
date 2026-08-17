#include "movement_module.hpp"

void MoveStepperOperation::start() noexcept { stepper_.move_to_steps(position_); }

Operation::PollResult MoveStepperOperation::poll() noexcept
{
    return stepper_.is_at_target() ? PollResult::finished : PollResult::running;
}

void MoveStepperOperation::cancel() noexcept { stepper_.stop_immediately(); }
