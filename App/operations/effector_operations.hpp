#ifndef EFFECTOR_OPERATIONS_HPP
#define EFFECTOR_OPERATIONS_HPP

#include "effector_driver.hpp"
#include "operation.hpp"
#include "tick.hpp"

class EffectorOperation : public Operation<Tick>
{
  public:
    EffectorOperation(EffectorDriver &effector, bool should_engage) noexcept
        : effector_{effector}, should_engage_(should_engage)
    {
    }

    void start(Tick & /*ctx*/) noexcept override
    {
        should_engage_ ? effector_.engage() : effector_.disengage();
    }
    PollResult poll(Tick & /*ctx*/) noexcept override { return PollResult::finished; }

  private:
    EffectorDriver &effector_;
    const bool should_engage_;
};

#endif // EFFECTOR_OPERATIONS_HPP
