#ifndef SEQUENCER_MODULES_HPP
#define SEQUENCER_MODULES_HPP

#include "sequencer_instructions.hpp"

extern void modules_start_instruction(SequencerInstruction instruction) noexcept;
extern void modules_dispatch_abort() noexcept;
extern void modules_dispatch_restart() noexcept;
extern SequencerInstructionState modules_instruction_state() noexcept;

#endif // SEQUENCER_MODULES_HPP
