#ifndef SEQUENCER_IMPL_HPP
#define SEQUENCER_IMPL_HPP

// The definitions of Sequencer. Include this only from the one file that explicitly
// instantiates Sequencer for a context type, next to the instantiation:
//
//     #include "sequencer_impl.hpp"
//     template class Sequencer<MyContext>;
//
// Everything else includes sequencer.hpp. A missing instantiation shows up as a link
// error such as `undefined reference to Sequencer<MyContext>::step(MyContext&)`.

#include "sequencer.hpp"

template <class Context>
void Sequencer<Context>::reset_operation_index() noexcept
{
    operation_index_ = 0;
}

template <class Context>
void Sequencer<Context>::advance_operation() noexcept
{
    operation_index_ = (operation_index_ + 1) % operations_.size();
}

template <class Context>
Operation<Context> &Sequencer<Context>::current_operation() const noexcept
{
    return *(operations_[operation_index_]);
}

template <class Context>
void Sequencer<Context>::start() noexcept
{
    is_sequencer_running_ = true;
}

template <class Context>
void Sequencer<Context>::stop() noexcept
{
    if (!is_sequencer_running_)
    {
        return;
    }

    is_sequencer_running_ = false;
    is_current_operation_started_ = false;
}

template <class Context>
void Sequencer<Context>::restart() noexcept
{
    is_current_operation_started_ = false;
    reset_operation_index();
    is_sequencer_running_ = true;
}

template <class Context>
void Sequencer<Context>::step(Context &ctx) noexcept
{
    if (!is_sequencer_running_)
    {
        return;
    }

    if (!is_current_operation_started_) // operation hasn't started
    {
        is_current_operation_started_ = true;
        current_operation().start(ctx);
        return;
    }

    // operation has been started
    switch (current_operation().poll(ctx))
    {
    case PollResult::running:
        // No-op - all work should be done in processing the modules
        break;

    case PollResult::finished:
        is_current_operation_started_ = false;
        advance_operation();
        break;

    case PollResult::aborted:
        // operation_index_ stays on it, so it can be read back
        stop();
        break;
    }
}

#endif // SEQUENCER_IMPL_HPP
