#ifndef EFFECTOR_OPERATIONS_HPP
#define EFFECTOR_OPERATIONS_HPP

#include "effector_driver.hpp"
#include "operation.hpp"
#include "tick.hpp"

#include <cstdint>

// Switches the effector, then waits for it to settle before the cycle moves on: the
// magnet needs time to grab the part, or to let go of it, and there is no feedback
// to wait on instead.
class EffectorOperation : public Operation<Tick>
{
  public:
    EffectorOperation(EffectorDriver &effector, bool should_engage,
                      const std::int32_t &settle_time_ms) noexcept
        : effector_{effector}, should_engage_(should_engage), settle_time_ms_{settle_time_ms}
    {
    }
    // Stored by reference, so a temporary would dangle.
    EffectorOperation(EffectorDriver &effector, bool should_engage,
                      const std::int32_t &&settle_time_ms) = delete;

    void start(Tick &ctx) noexcept override
    {
        start_time_ = ctx.now_ms;
        active_settle_time_ms_ = static_cast<std::uint32_t>(settle_time_ms_);
        should_engage_ ? effector_.engage() : effector_.disengage();
    }

    PollResult poll(Tick &ctx) noexcept override
    {
        const std::uint32_t elapsed = ctx.now_ms - start_time_;

        return elapsed >= active_settle_time_ms_ ? PollResult::finished : PollResult::running;
    }

  private:
    EffectorDriver &effector_;
    const bool should_engage_;
    const std::int32_t &settle_time_ms_;
    std::uint32_t active_settle_time_ms_ = 0; // Snapshot taken in start()
    std::uint32_t start_time_ = 0;
};

#endif // EFFECTOR_OPERATIONS_HPP
