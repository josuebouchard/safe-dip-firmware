#ifndef TIMER_OPERATIONS_HPP
#define TIMER_OPERATIONS_HPP

#include "operation.hpp"
#include <cstdint>

class DwellOperation : public Operation
{
  public:
    DwellOperation(std::uint32_t &dwell_time_ms) noexcept : dwell_time_ms_{dwell_time_ms} {}

    void start() noexcept override;
    PollResult poll() noexcept override;

  private:
    std::uint32_t &dwell_time_ms_;
    std::uint32_t start_time_ = 0;
};

#endif // TIMER_OPERATIONS_HPP
