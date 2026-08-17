#include "sequencer.hpp"

void Sequencer::reset_instruction_index() noexcept { instruction_index_ = 0; }

void Sequencer::advance_instruction() noexcept
{
    instruction_index_ = (instruction_index_ + 1) % instructions_.size();
}

Operation &Sequencer::current_instruction() const noexcept
{
    return *(instructions_[instruction_index_]);
}

void Sequencer::abort() noexcept { system_status_ = SequencerSystemStatus::stopping; }

void Sequencer::sequencer_step() noexcept
{
    switch (system_status_)
    {
    case SequencerSystemStatus::running:
        run_current_instruction();
        break;
    case SequencerSystemStatus::stopping:
        current_instruction().cancel();
        reset_instruction_index();
        system_status_ = SequencerSystemStatus::stopped;
        is_current_instruction_started_ = false;
        break;
    case SequencerSystemStatus::stopped:
        // No-op
        break;
    default:
        // TODO: Should be unreachable
        break;
    }
}

void Sequencer::run_current_instruction() noexcept
{
    if (!is_current_instruction_started_) // Instruction hasn't started
    {
        is_current_instruction_started_ = true;
        current_instruction().start();
        return;
    }
    else // Instruction has been started
    {
        switch (current_instruction().poll())
        {
        case Operation::PollResult::running:
            // No-op - all work should be done in processing the modules
            break;

        case Operation::PollResult::finished:
            is_current_instruction_started_ = false;
            advance_instruction();
            break;
        }
    }
}
