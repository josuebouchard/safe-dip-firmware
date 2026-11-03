#ifndef OPERATION_HPP
#define OPERATION_HPP

// One step of the cycle. An implementation must:
// - Read recipe values only in start(), keeping a copy if poll() needs them later.
// - Reset all per-run state in start(): the same object runs several times per
//   cycle, and again after a stop.
// - Tolerate being dropped between any two polls, with no cleanup call.
// - Never block in start() or poll().
// - Not assume it alone controls its actuator: loop() can stop it at any time.
class Operation
{
  public:
    enum class PollResult
    {
        running,
        finished,
        aborted, // Could not complete - the sequence must not continue past it
    };

    virtual void start() noexcept = 0;
    virtual PollResult poll() noexcept = 0;

    virtual ~Operation() = default;
};

#endif // OPERATION_HPP
