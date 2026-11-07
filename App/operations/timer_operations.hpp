#ifndef TIMER_OPERATIONS_HPP
#define TIMER_OPERATIONS_HPP

#include "operation.hpp"
#include "tick.hpp"
#include <cstdint>

class DwellOperation : public Operation<Tick>
{
  public:
    explicit DwellOperation(const std::int32_t &dwell_time_ms) noexcept
        : dwell_time_ms_{dwell_time_ms}
    {
    }
    // Stored by reference, so a temporary would dangle.
    explicit DwellOperation(const std::int32_t &&dwell_time_ms) = delete;

    void start(Tick &ctx) noexcept override;
    PollResult poll(Tick &ctx) noexcept override;

  private:
    const std::int32_t &dwell_time_ms_;
    std::uint32_t active_dwell_time_ms_ = 0; // Snapshot taken in start()
    std::uint32_t start_time_ = 0;
};

#endif // TIMER_OPERATIONS_HPP
