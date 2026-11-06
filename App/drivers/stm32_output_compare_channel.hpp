#ifndef STM32_OUTPUT_COMPARE_CHANNEL_HPP
#define STM32_OUTPUT_COMPARE_CHANNEL_HPP

#include "stm32f4xx_hal.h"
#include <cstdint>

class STM32OutputCompareChannel
{
  public:
    struct Config
    {
        TIM_HandleTypeDef *timer;
        std::uint32_t timer_channel;
        // TIM_OCPOLARITY_LOW for a sinking (open-drain) output: the inactive
        // OC reference then releases the pin and the opto stays dark.
        std::uint32_t polarity;
    };

    STM32OutputCompareChannel(Config config) : config_(config) {}

    // initial_offset_ticks schedules the first compare match relative to the
    // timer's current (free-running) counter value.
    void start_interrupt(std::uint32_t initial_offset_ticks) noexcept;
    void stop_interrupt() noexcept;

    [[nodiscard]] std::uint32_t counter() const noexcept;
    [[nodiscard]] std::uint32_t compare() const noexcept;

    void set_compare(std::uint32_t value) noexcept;

  private:
    const Config config_;
};

#endif // STM32_OUTPUT_COMPARE_CHANNEL_HPP
