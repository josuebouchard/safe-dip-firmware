#include "stepper_driver.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>

void StepperDriver::on_compare_elapsed() noexcept
{
    // Toggle step output flag to match the compare reference
    is_step_output_active_ = !is_step_output_active_;

    // Count - only on the active edge
    if (is_step_output_active_)
    {
        pulse_count_.fetch_add(static_cast<int8_t>(direction_current_), std::memory_order_relaxed);
    }
    else // On the inactive edge
    {
        // Load requested speed
        const auto speed_requested = speed_requested_.load(std::memory_order_relaxed);

        if (speed_requested == 0) // Graceful stop
        {
            return; // Interrupt should be cancelled from main
        }

        apply_speed(speed_requested);
    }

    // Set next compare value
    const std::uint32_t previous_compare = config_.timer_channel.compare();
    config_.timer_channel.set_compare(previous_compare + half_period_ticks_);
}

void StepperDriver::set_speed(std::int32_t steps_per_second) noexcept
{
    // Clamp to what the timer can actually resolve. Without this, an
    // over-ceiling request makes the half-period truncate to 0 and the ISR
    // stops rescheduling the compare - silently killing a running pulse train.
    const std::int32_t speed_limit = max_steps_per_second();
    steps_per_second = std::clamp(steps_per_second, -speed_limit, speed_limit);

    if (steps_per_second == 0)
    {
        // The channel is disarmed, so no further ISR will run to publish the stop.
        stop_immediately();
        return;
    }

    // Coming back from stopped. Gated on is_armed_ rather than on any
    // ISR-published speed: repeated calls from a tight loop before the ISR ever
    // runs would otherwise re-arm the channel from scratch every time, so it
    // would never survive long enough to fire.
    if (!is_armed_)
    {
        // Seed the period/direction/edge state before arming the interrupt,
        // since on_compare_elapsed() only recomputes them on inactive edges
        // and the first call is always an active edge.
        apply_speed(steps_per_second);
        is_step_output_active_ = false;

        config_.timer_channel.start_interrupt(half_period_ticks_);
        is_armed_ = true;
    }

    speed_requested_.store(steps_per_second, std::memory_order_release);
}

void StepperDriver::stop_immediately() noexcept
{
    // Already stopped: the channel is disarmed and parked, so there is nothing to
    // reconfigure. loop() calls this on every pass while a fault is latched.
    if (!is_armed_)
    {
        return;
    }

    config_.timer_channel.stop_interrupt();
    speed_requested_.store(0, std::memory_order_release);
    is_armed_ = false;
}

// speed must be non-zero and clamped by set_speed().
void StepperDriver::apply_speed(std::int32_t speed) noexcept
{
    // @todo: assert(speed != 0 && std::abs(speed) <= max_steps_per_second())
    half_period_ticks_ =
        config_.timer_ticks_per_second / (2U * static_cast<std::uint32_t>(std::abs(speed)));
    direction_current_ = speed >= 0 ? Direction::clockwise : Direction::counterclockwise;
    // @todo: assert(half_period_ticks_ != 0)
    write_direction_pin(direction_current_);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void StepperDriver::write_direction_pin(Direction direction) noexcept
{
    HAL_GPIO_WritePin(config_.direction.port, config_.direction.pin,
                      direction == Direction::clockwise
                          ? config_.direction.electrical_state_clockwise
                          : opposite(config_.direction.electrical_state_clockwise));
}
