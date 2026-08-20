#include "timer_operations.hpp"
#include "stm32f4xx_hal.h"

void DwellOperation::start() noexcept { start_time_ = HAL_GetTick(); }

DwellOperation::PollResult DwellOperation::poll() noexcept
{
    const uint32_t elapsed = HAL_GetTick() - start_time_;

    return elapsed >= dwell_time_ms_ ? PollResult::finished : PollResult::running;
}