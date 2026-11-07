#ifndef OPERATION_HPP
#define OPERATION_HPP

enum class PollResult
{
    running,
    finished,
    aborted, // Could not complete - the sequence must not continue past it
};

// Why poll() returned aborted.
enum class Failure
{
    unspecified,
    interrupted,     // Something stopped it before it completed
    timed_out,       // Ran for longer than allowed
    x_homing_failed, // The X switch misread: stuck, or hit far from where it should be
    y_homing_failed, // Same, for Y
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
// - Before returning aborted, stop any actuator that would otherwise keep going.
template <class Context>
class Operation
{
  public:
    virtual void start(Context &ctx) noexcept = 0;
    virtual PollResult poll(Context &ctx) noexcept = 0;

    // Why poll() returned aborted. Only meaningful after an aborted result.
    [[nodiscard]] virtual Failure failure() const noexcept { return Failure::unspecified; }

    virtual ~Operation() = default;
};

#endif // OPERATION_HPP
