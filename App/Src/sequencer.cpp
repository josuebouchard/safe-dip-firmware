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
    if (system_status_ == SequencerSystemStatus::stopped)
    {
        system_status_ = SequencerSystemStatus::running;
    }
}

void Sequencer::abort() noexcept { system_status_ = SequencerSystemStatus::stopping; }

void Sequencer::sequencer_step() noexcept
{
    switch (system_status_)
    {
    case SequencerSystemStatus::running:
        run_current_operation();
        break;
    case SequencerSystemStatus::stopping:
        current_operation().cancel();
        system_status_ = SequencerSystemStatus::stopped;
        is_current_operation_started_ = false;
        reset_operation_index();
        break;
    case SequencerSystemStatus::stopped:
        // No-op
        break;
    default:
        // TODO: Should be unreachable
        break;
    }
}

void Sequencer::run_current_operation() noexcept
{
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
