#ifndef OPERATION_HPP
#define OPERATION_HPP

enum class PollResult
{
    running,
    finished,
    aborted, // The sequence must not continue past it: the sequencer stops on it
};

// One step of the cycle. `Context` is what the user of the sequencer passes on every
// pass; the sequencer only forwards it. An implementation must:
// - Read recipe values only in start(), keeping a copy if poll() needs them later.
// - Reset all per-run state in start(): the same object runs several times per
//   cycle, and again after a stop.
// - Tolerate being dropped between any two polls, with no cleanup call.
// - Never block in start() or poll().
// - Take the time from the context, and never read a clock.
// - Not assume it alone controls its actuator: loop() can stop it at any time.
// - Report why it couldn't complete through the context, before returning aborted.
//   Returning aborted without a reason is for a deliberate stop that isn't an error,
//   and nothing else stops the actuators then: leave them as they should stay.
// - Never return finished after reporting a failure.
template <class Context>
class Operation
{
  public:
    virtual void start(Context &ctx) noexcept = 0;
    virtual PollResult poll(Context &ctx) noexcept = 0;

    virtual ~Operation() = default;
};

#endif // OPERATION_HPP
