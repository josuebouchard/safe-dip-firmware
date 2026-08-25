#include "stepper_driver.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>

void StepperDriver::on_compare_elapsed() noexcept
{
    // Toggle step output flag to match pin output
    is_step_output_high_ = !is_step_output_high_;

    // Count - only on rising edge
    if (is_step_output_high_)
    {
        pulse_count_.fetch_add(static_cast<int8_t>(direction_current_), std::memory_order_relaxed);
    }
    else // On falling edge
    {
        // Load requested speed
        const auto speed_requested = speed_requested_.load(std::memory_order_relaxed);

        if (speed_requested == 0) // Graceful stop
        {
            speed_current_ = 0;
            return; // Interrupt should be cancelled from main
        }

        // Convert speed to direction and velocity
        const auto velocity_requested = std::abs(speed_requested);
        const auto direction_requested =
            speed_requested >= 0 ? Direction::clockwise : Direction::counterclockwise;

        // Calculate half_period_ticks
        if (!set_half_period_ticks(config_.timer_ticks_per_second / (2U * velocity_requested)))
        {
            return; // @todo: should be unreachable, speed_requested == 0 handled above
        }

        // Store new speed and direction
        direction_current_ = direction_requested;
        speed_current_.store(speed_requested, std::memory_order_relaxed);

        // Write GPIO DIR
        write_direction_pin(direction_current_);
    }

    // Set next compare value
    const std::uint32_t previous_compare = config_.timer_channel.compare();
    config_.timer_channel.set_compare(previous_compare + half_period_ticks_);
}

void StepperDriver::set_speed(std::int32_t steps_per_second) noexcept
{
    // Comming back from stopped
    if (is_stopped() && steps_per_second != 0)
    {
        // Seed the period/direction/edge state before arming the interrupt,
        // since on_compare_elapsed() only recomputes them on falling edges
        // and the first call is always a rising edge.
        const auto velocity_requested = static_cast<std::uint32_t>(std::abs(steps_per_second));
        if (!set_half_period_ticks(config_.timer_ticks_per_second / (2U * velocity_requested)))
        {
            return; // @todo: should be unreachable, steps_per_second != 0 guaranteed above
        }

        direction_current_ =
            steps_per_second >= 0 ? Direction::clockwise : Direction::counterclockwise;
        write_direction_pin(direction_current_);
        is_step_output_high_ = false;

        config_.timer_channel.start_interrupt(half_period_ticks_);
    }
    else if (steps_per_second == 0)
    {
        config_.timer_channel.stop_interrupt();
    }

    speed_requested_.store(steps_per_second, std::memory_order_release);
}

void StepperDriver::stop_immediately() noexcept
{
    config_.timer_channel.stop_interrupt();
    speed_requested_.store(0, std::memory_order_release);
    speed_current_.store(0, std::memory_order_relaxed);
}

bool StepperDriver::set_half_period_ticks(std::uint32_t half_period_ticks) noexcept
{
    if (half_period_ticks == 0)
    {
        return false;
    }

    half_period_ticks_ = half_period_ticks;
    return true;
}

void StepperDriver::write_direction_pin(Direction direction) noexcept
{
    HAL_GPIO_WritePin(config_.direction.port, config_.direction.pin,
                      direction == Direction::clockwise
                          ? config_.direction.electrical_state_clockwise
                          : opposite(config_.direction.electrical_state_clockwise));
}