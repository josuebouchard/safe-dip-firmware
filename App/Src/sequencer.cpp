#include "sequencer.hpp"

void Sequencer::reset_operation_index() noexcept { operation_index_ = 0; }

void Sequencer::advance_operation() noexcept
{
    operation_index_ = (operation_index_ + 1) % operations_.size();
}

Operation &Sequencer::current_operation() const noexcept
{
    return *(operations_[operation_index_]);
}

void Sequencer::start() noexcept
{
    if (!is_system_running_)
        is_system_running_ = true;
}

void Sequencer::stop() noexcept
{
    if (!is_system_running_)
        return;

    is_system_running_ = false;
    is_current_operation_started_ = false;
}

void Sequencer::restart() noexcept
{
    is_current_operation_started_ = false;
    reset_operation_index();
    is_system_running_ = true;
}

bool Sequencer::is_running() const noexcept { return is_system_running_; }

void Sequencer::sequencer_step() noexcept
{
    if (!is_system_running_)
    {
        return;
    }

    if (!is_current_operation_started_) // operation hasn't started
    {
        is_current_operation_started_ = true;
        current_operation().start();
        return;
    }
    else // operation has been started
    {
        switch (current_operation().poll())
        {
        case Operation::PollResult::running:
            // No-op - all work should be done in processing the modules
            break;

        case Operation::PollResult::finished:
            is_current_operation_started_ = false;
            advance_operation();
            break;
        }
    }
}