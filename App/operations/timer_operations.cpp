#include "timer_operations.hpp"
#include "stm32f4xx_hal.h"

void DwellOperation::start() noexcept
{
    start_time_ = HAL_GetTick();
    active_dwell_time_ms_ = static_cast<std::uint32_t>(dwell_time_ms_);
}

DwellOperation::PollResult DwellOperation::poll() noexcept
{
    const uint32_t elapsed = HAL_GetTick() - start_time_;

    return elapsed >= active_dwell_time_ms_ ? PollResult::finished : PollResult::running;
}