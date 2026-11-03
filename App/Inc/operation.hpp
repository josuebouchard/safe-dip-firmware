#ifndef OPERATION_HPP
#define OPERATION_HPP

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
