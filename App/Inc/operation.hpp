#ifndef OPERATION_HPP
#define OPERATION_HPP

class Operation
{
  public:
    enum class PollResult
    {
        running,
        finished,
    };

    virtual void start() noexcept = 0;
    virtual PollResult poll() noexcept = 0;
    virtual void cancel() noexcept = 0;
};

#endif // OPERATION_HPP
