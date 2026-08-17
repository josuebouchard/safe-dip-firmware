#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include <cstddef>
#include <span>

#include "operation.hpp"
#include "sequencer_instructions.hpp"

// === SEQUENCER STATE MACHINE ===

class Sequencer
{
  public:
    explicit Sequencer(const std::span<Operation *> instructions)
        : system_status_(SequencerSystemStatus::stopped), instructions_(instructions) {};

    void sequencer_step() noexcept;
    void abort() noexcept;

  private:
    enum class SequencerSystemStatus
    {
        running,
        stopping,
        stopped
    };

    SequencerSystemStatus system_status_;

    const std::span<Operation *> instructions_;
    std::size_t instruction_index_ = 0;
    bool is_current_instruction_started_ = false;

    void reset_instruction_index() noexcept;
    void advance_instruction() noexcept;
    Operation &current_instruction() const noexcept;

    void run_current_instruction() noexcept;
};

// === EXTERNAL FUNCTIONS ===

extern void modules_start_instruction(SequencerInstruction instruction) noexcept;
extern void modules_dispatch_abort() noexcept;
extern void modules_dispatch_restart() noexcept;
extern SequencerInstructionState modules_instruction_state() noexcept;

#endif // SEQUENCER_HPP
