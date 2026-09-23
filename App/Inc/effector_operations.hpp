#ifndef EFFECTOR_OPERATIONS_HPP
#define EFFECTOR_OPERATIONS_HPP

#include "effector_driver.hpp"
#include "operation.hpp"

class EffectorOperation : public Operation
{
  public:
    EffectorOperation(EffectorDriver &effector, bool should_engage) noexcept
        : effector_{effector}, should_engage_(should_engage)
    {
    }

    void start() noexcept override;
    PollResult poll() noexcept override { return PollResult::finished; }

  protected:
    EffectorDriver &effector_;
    const bool should_engage_;
};

#endif // EFFECTOR_OPERATIONS_HPP
