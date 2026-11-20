#ifndef TRIGGER_OPERATIONS_HPP
#define TRIGGER_OPERATIONS_HPP

#include "operation.hpp"
#include "tick.hpp"

// Holds the cycle until the trigger pin reads low, then lets it continue. Has no
// timeout: it waits for as long as the pin stays high.
class WaitForTriggerOperation : public Operation<Tick>
{
  public:
    void start(Tick & /*ctx*/) noexcept override {}

    PollResult poll(Tick &ctx) noexcept override
    {
        return ctx.inputs.trigger_active ? PollResult::finished : PollResult::running;
    }
};

#endif // TRIGGER_OPERATIONS_HPP
