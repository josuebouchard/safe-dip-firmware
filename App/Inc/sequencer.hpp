#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include <cstddef>
#include <span>

#include "operation.hpp"

// === SEQUENCER STATE MACHINE ===

class Sequencer
{
  public:
    explicit Sequencer(const std::span<Operation *> operations) : operations_(operations) {};

    void sequencer_step() noexcept;
    void start() noexcept;
    void stop() noexcept;

    bool is_running() const noexcept;

  private:
    bool is_system_running_ = false;

    const std::span<Operation *> operations_;
    std::size_t operation_index_ = 0;
    bool is_current_operation_started_ = false;

    void reset_operation_index() noexcept;
    void advance_operation() noexcept;
    Operation &current_operation() const noexcept;

    void run_current_operation() noexcept;
};

#endif // SEQUENCER_HPP
