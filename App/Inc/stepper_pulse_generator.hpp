#ifndef STEPPER_PULSE_GENERATOR_HPP
#define STEPPER_PULSE_GENERATOR_HPP

#include "stm32_output_compare_channel.hpp"
#include "stm32f4xx_hal.h"

#include <atomic>
#include <cstdint>

class StepperPulseGenerator final
{
  public:
    enum class Direction : std::uint8_t
    {
        clockwise,
        counterclockwise,
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

        double timer_tick_period_ms;

        DirectionPin direction;
        std::uint32_t timer_ticks_per_ms;
    };

    explicit StepperPulseGenerator(const Config &config) noexcept : config_{config} {};

    // Call after MX_GPIO_Init() and MX_TIMx_Init().
    void initialize(Direction initial_direction = Direction::clockwise) noexcept;

    // Direction may only be changed on initialized.
    [[nodiscard]] bool set_direction(Direction direction) noexcept;

    // Begin continuous STEP pulse generation.
    [[nodiscard]] bool start(std::uint32_t half_period_ticks) noexcept;

    // Issue a stop request, satisfied as soon as STEP is low
    void stop() noexcept;

    // The new period takes effect the next time the ISR
    // schedules a compare event.
    [[nodiscard]] bool set_half_period_ticks(std::uint32_t half_period_ticks) noexcept;

    [[nodiscard]] bool is_running() const noexcept
    {
        return running_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] Direction direction() const noexcept { return direction_; }

    [[nodiscard]] std::int32_t pulse_count() const noexcept
    {
        return pulse_count_.load(std::memory_order_relaxed);
    }

    // Function to be called in ISR
    void on_compare_elapsed() noexcept;

    bool set_speed(double microsteps_per_ms) noexcept;

  private:
    const Config config_;

    bool initialized_{false};
    Direction direction_{Direction::clockwise};

    std::atomic<std::uint32_t> half_period_ticks_{0U};
    std::atomic<std::int32_t> pulse_count_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_flag_{false};
    std::atomic<bool> step_output_high_{false};

    static GPIO_PinState opposite(GPIO_PinState state) noexcept
    {
        return state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    static void set_compare_register(TIM_HandleTypeDef *timer, std::uint32_t channel,
                                     std::uint32_t value) noexcept;

    void stop_channel_now() noexcept;

    // Write pins
    void write_direction(Direction direction) noexcept;
};

extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *timer);

#endif // STEPPER_PULSE_GENERATOR_HPP
