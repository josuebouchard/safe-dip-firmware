#ifndef EFFECTOR_OPERATIONS_HPP
#define EFFECTOR_OPERATIONS_HPP

#include "effector_driver.hpp"
#include "operation.hpp"

class EffectorOperation : public Operation
{
  public:
    EffectorOperation(EffectorDriver &effector) noexcept : effector_{effector} {}

    PollResult poll() noexcept override { return PollResult::finished; }
    void cancel() noexcept override { effector_.engage(); };

  protected:
    EffectorDriver &effector_;
};

class EffectorEngageOperation : public EffectorOperation
{
  public:
    explicit EffectorEngageOperation(EffectorDriver &effector) noexcept
        : EffectorOperation{effector}
    {
    }

    void start() noexcept override;
};

class EffectorDisengageOperation : public EffectorOperation
{
  public:
    explicit EffectorDisengageOperation(EffectorDriver &effector) noexcept
        : EffectorOperation{effector}
    {
    }

    void start() noexcept override;
};

#endif // EFFECTOR_OPERATIONS_HPP
