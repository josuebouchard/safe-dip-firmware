#include "timer_operations.hpp"

void DwellOperation::start(Tick &ctx) noexcept
{
    start_time_ = ctx.now_ms;
    active_dwell_time_ms_ = static_cast<std::uint32_t>(dwell_time_ms_);
}

PollResult DwellOperation::poll(Tick &ctx) noexcept
{
    const std::uint32_t elapsed = ctx.now_ms - start_time_;

    return elapsed >= active_dwell_time_ms_ ? PollResult::finished : PollResult::running;
}