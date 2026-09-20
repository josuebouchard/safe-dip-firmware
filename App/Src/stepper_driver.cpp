#include "stepper_driver.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
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
            // Unreachable: set_speed() clamps before storing speed_requested_,
            // so the half-period is always >= 1 tick. Returning here would
            // leave the compare unscheduled and kill the pulse train.
            return;
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
    // Clamp to what the timer can actually resolve. Without this, an
    // over-ceiling request makes half_period_ticks truncate to 0: while
    // already armed the guarded block below is skipped, so the bad value
    // reaches the ISR, whose own set_half_period_ticks() then fails and
    // returns without rescheduling the compare - silently killing a
    // running pulse train.
    const std::int32_t speed_limit = max_steps_per_second();
    steps_per_second = std::clamp(steps_per_second, -speed_limit, speed_limit);

    // Comming back from stopped. Gated on is_armed_ rather than is_stopped()
    // (which only reflects the ISR's last confirmed speed): if set_speed()
    // is called repeatedly before the ISR ever gets a chance to run - e.g.
    // from a tight loop - is_stopped() would stay true and every call would
    // re-arm/reconfigure the channel from scratch, so it would never survive
    // long enough to actually fire.
    if (!is_armed_ && steps_per_second != 0)
    {
        // Seed the period/direction/edge state before arming the interrupt,
        // since on_compare_elapsed() only recomputes them on inactive edges
        // and the first call is always an active edge.
        const auto velocity_requested = static_cast<std::uint32_t>(std::abs(steps_per_second));
        if (!set_half_period_ticks(config_.timer_ticks_per_second / (2U * velocity_requested)))
        {
            return; // Unreachable: the clamp above keeps the half-period >= 1 tick.
        }

        direction_current_ =
            steps_per_second >= 0 ? Direction::clockwise : Direction::counterclockwise;
        write_direction_pin(direction_current_);
        is_step_output_active_ = false;

        config_.timer_channel.start_interrupt(half_period_ticks_);
        is_armed_ = true;
    }
    else if (steps_per_second == 0)
    {
        config_.timer_channel.stop_interrupt();
        is_armed_ = false;

        // Publish the stop here: the ISR normally clears speed_current_ on
        // its next inactive edge, but we just disarmed the channel so no
        // further ISR will run - leaving speed()/is_stopped() reporting the
        // last moving speed forever.
        speed_current_.store(0, std::memory_order_relaxed);
    }

    speed_requested_.store(steps_per_second, std::memory_order_release);
}

void StepperDriver::stop_immediately() noexcept
{
    config_.timer_channel.stop_interrupt();
    speed_requested_.store(0, std::memory_order_release);
    speed_current_.store(0, std::memory_order_relaxed);
    is_armed_ = false;
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