#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include <cstddef>
#include <span>

#include "operation.hpp"

// === SEQUENCER STATE MACHINE ===

// Runs a list of operations in order, passing `Context` through to each one.
//
// Declarations only. The definitions are in sequencer_impl.hpp, which is compiled
// once, by the file that explicitly instantiates Sequencer for its context type
// (`template class Sequencer<MyContext>;`). Every other file includes only this header.
template <class Context>
class Sequencer
{
  public:
    explicit Sequencer(std::span<Operation<Context> *> operations) noexcept
        : operations_(operations)
    {
    }

    // Starts or polls the current operation, and stops on it if it aborts. Why it
    // aborted is the operation's to report, through the context.
    void step(Context &ctx) noexcept;
    void start() noexcept;
    void stop() noexcept;
    void restart() noexcept;

    [[nodiscard]] bool is_running() const noexcept { return is_sequencer_running_; }
    [[nodiscard]] std::size_t operation_index() const noexcept { return operation_index_; }

  private:
    bool is_sequencer_running_ = false;

    const std::span<Operation<Context> *> operations_;
    std::size_t operation_index_ = 0;
    bool is_current_operation_started_ = false;

    void reset_operation_index() noexcept;
    void advance_operation() noexcept;
    Operation<Context> &current_operation() const noexcept;
};

#endif // SEQUENCER_HPP
