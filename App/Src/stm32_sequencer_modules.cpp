#include "sequencer_modules.hpp"

void modules_start_instruction(SequencerInstruction instruction) noexcept
{
    switch (instruction)
    {

    case SequencerInstruction::move_x_home:
    case SequencerInstruction::move_x_dip:
    case SequencerInstruction::move_x_output:

    case SequencerInstruction::move_z_up:
    case SequencerInstruction::move_z_down:

    case SequencerInstruction::effector_engage:
    case SequencerInstruction::effector_disengage:

    case SequencerInstruction::timer_dwell:
        break;
    }
};
void modules_dispatch_abort() noexcept {

};
void modules_dispatch_restart() noexcept {

};
SequencerInstructionState modules_instruction_state() noexcept
{
    return SequencerInstructionState::idle;
};