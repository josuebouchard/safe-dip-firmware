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
    };

    STM32OutputCompareChannel(Config config) : config_(config) {}

    void start_interrupt() noexcept;
    void stop_interrupt() noexcept;

    std::uint32_t counter() const noexcept;
    std::uint32_t compare() const noexcept;

    void set_compare(std::uint32_t value) noexcept;

  private:
    const Config config_;
};

#endif // STM32_OUTPUT_COMPARE_CHANNEL_HPP
