#ifndef TIMER_OPERATIONS_HPP
#define TIMER_OPERATIONS_HPP

#include "operation.hpp"
#include <cstdint>

class DwellOperation : public Operation
{
  public:
    explicit DwellOperation(std::uint32_t dwell_time_ms) noexcept : dwell_time_ms_{dwell_time_ms} {}

    void start() noexcept override;
    PollResult poll() noexcept override;

    // Takes effect on the next start()
    void set_dwell_time(std::uint32_t dwell_time_ms) noexcept { dwell_time_ms_ = dwell_time_ms; }
    std::uint32_t dwell_time() const noexcept { return dwell_time_ms_; }

  private:
    std::uint32_t dwell_time_ms_;
    std::uint32_t active_dwell_time_ms_ = 0; // Snapshot taken in start()
    std::uint32_t start_time_ = 0;
};

#endif // TIMER_OPERATIONS_HPP
