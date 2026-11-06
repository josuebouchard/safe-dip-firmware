#ifndef OPERATION_HPP
#define OPERATION_HPP

// One step of the cycle. An implementation must:
// - Read recipe values only in start(), keeping a copy if poll() needs them later.
// - Reset all per-run state in start(): the same object runs several times per
//   cycle, and again after a stop.
// - Tolerate being dropped between any two polls, with no cleanup call.
// - Never block in start() or poll().
// - Not assume it alone controls its actuator: loop() can stop it at any time.
// - Before returning aborted, stop any actuator that would otherwise keep going.
class Operation
{
  public:
    enum class PollResult
    {
        running,
        finished,
        aborted, // Could not complete - the sequence must not continue past it
    };

    enum class Failure
    {
        unspecified,
        interrupted,      // Something stopped it before it completed
        timed_out,        // Ran for longer than allowed
        sensor_stuck,     // A sensor kept reading a state it should have left
        out_of_tolerance, // A measured value was too far from the expected one
    };

    virtual void start() noexcept = 0;
    virtual PollResult poll() noexcept = 0;

    // Why poll() returned aborted. Only meaningful after an aborted result.
    [[nodiscard]] virtual Failure failure() const noexcept { return Failure::unspecified; }

    virtual ~Operation() = default;
};

#endif // OPERATION_HPP
