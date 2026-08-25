#ifndef STEPPER_DRIVER_HPP
#define STEPPER_DRIVER_HPP

#include "stm32_output_compare_channel.hpp"
#include "stm32f4xx_hal.h"

#include <atomic>
#include <cstdint>

class StepperDriver
{
  public:
    enum class Direction : std::int8_t
    {
        clockwise = 1,
        counterclockwise = -1,
    };

    struct DirectionPin
    {
        GPIO_TypeDef *port;
        std::uint16_t pin;

        // Electrical state corresponding to Direction::clockwise.
        GPIO_PinState electrical_state_clockwise;
    };

    struct Config
    {
        STM32OutputCompareChannel &timer_channel;

        DirectionPin direction;
        std::uint32_t timer_ticks_per_second;
    };

    explicit StepperDriver(const Config &config) noexcept : config_{config} {};

    // Function to be called in ISR
    void on_compare_elapsed() noexcept;

    void set_speed(std::int32_t steps_per_second) noexcept;

    void stop_immediately() noexcept;

    std::int32_t speed() const noexcept { return speed_current_.load(std::memory_order_relaxed); }

    bool is_stopped() const noexcept { return speed() == 0; }

    bool is_running() const noexcept { return !is_stopped(); }

    [[nodiscard]] Direction direction() const noexcept
    {
        return speed() >= 0 ? Direction::clockwise : Direction::counterclockwise;
    }

    [[nodiscard]] std::int32_t pulse_count() const noexcept
    {
        return pulse_count_.load(std::memory_order_relaxed);
    }

  private:
    const Config config_;

    // Shared
    std::atomic<std::int32_t> speed_requested_{0};
    std::atomic<std::int32_t> speed_current_{0};
    std::atomic<std::int32_t> pulse_count_{0};

    // ISR owned
    std::uint32_t last_tick{0};
    Direction direction_current_{Direction::clockwise};
    std::uint32_t half_period_ticks_{0U};
    bool is_step_output_high_{false};

    static GPIO_PinState opposite(GPIO_PinState state) noexcept
    {
        return state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    [[nodiscard]] bool set_half_period_ticks(std::uint32_t half_period_ticks) noexcept;

    void stop_channel_now() noexcept;

    // Write pins
    void write_direction_pin(Direction direction) noexcept;
};

#endif // STEPPER_DRIVER_HPP
