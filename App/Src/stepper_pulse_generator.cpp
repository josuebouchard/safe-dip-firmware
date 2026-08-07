#include "stepper_pulse_generator.hpp"
#include <atomic>
#include <cmath>
#include <cstdlib>

void StepperPulseGenerator::initialize(Direction initial_direction) noexcept
{
    running_.store(false, std::memory_order_relaxed);
    stop_requested_flag_.store(false, std::memory_order_relaxed);
    step_output_high_.store(false, std::memory_order_relaxed);

    pulse_count_.store(0U, std::memory_order_relaxed);
    half_period_ticks_.store(0U, std::memory_order_relaxed);

    direction_ = initial_direction;
    write_direction(initial_direction);

    initialized_ = true;
}

bool StepperPulseGenerator::set_direction(Direction direction) noexcept
{
    if (!initialized_ || is_running())
        return false;

    direction_ = direction;
    write_direction(direction);
    return true;
}

bool StepperPulseGenerator::start(std::uint32_t half_period_ticks) noexcept
{
    if (!initialized_ || is_running() || half_period_ticks == 0U)
        return false;

    // A normal stop should always leave STEP low.
    if (step_output_high_.load(std::memory_order_relaxed))
        return false;

    // Set half ticks
    half_period_ticks_.store(half_period_ticks, std::memory_order_relaxed);

    // Reset flags
    stop_requested_flag_.store(false, std::memory_order_relaxed);

    // Set timer
    const std::uint32_t now = config_.timer_channel.counter();
    config_.timer_channel.set_compare(now + half_period_ticks);

    // From now on, the generator is considered running
    // Serves as synchronization point
    running_.store(true, std::memory_order_release);

    // Start timer
    config_.timer_channel.start_interrupt();

    return true;
}

void StepperPulseGenerator::stop() noexcept
{
    if (!is_running())
        return;

    // If STEP is low, the output can stop immediately.
    if (!step_output_high_.load(std::memory_order_relaxed))
        stop_channel_now();
    // If STEP is high, request a stop after the falling edge.
    else
        stop_requested_flag_.store(true, std::memory_order_relaxed);
}

bool StepperPulseGenerator::set_speed(double microsteps_per_ms) noexcept
{
    if (!initialized_)
        return false;

    if (microsteps_per_ms == 0.0)
    {
        stop();
        return true;
    }

    const auto desired_direction =
        microsteps_per_ms > 0.0 ? Direction::clockwise : Direction::counterclockwise;

    // Reverse only after the current pulse train has completely stopped.
    if (desired_direction != direction_)
    {
        if (is_running())
        {
            stop();
            return false;
        }

        if (!set_direction(desired_direction))
            return false;
    }

    const double magnitude = std::abs(microsteps_per_ms);

    const double half_period_ticks = 1.0 / (2.0 * magnitude * config_.timer_tick_period_ms);

    if (half_period_ticks < 1.0)
        return false;

    const auto ticks = static_cast<std::uint32_t>(std::round(half_period_ticks));

    (void)set_half_period_ticks(ticks);

    if (!is_running())
        return start(ticks);

    return true;
}

void StepperPulseGenerator::stop_channel_now() noexcept
{
    // Stop running
    running_.store(false, std::memory_order_relaxed);

    config_.timer_channel.stop_interrupt();

    // Reset stop flag
    stop_requested_flag_.store(false, std::memory_order_relaxed);
}

bool StepperPulseGenerator::set_half_period_ticks(std::uint32_t half_period_ticks) noexcept
{
    if (half_period_ticks == 0U)
        return false;

    half_period_ticks_.store(half_period_ticks, std::memory_order_relaxed);
    return true;
}

void StepperPulseGenerator::on_compare_elapsed() noexcept
{
    if (!is_running())
        return;

    // Toggle step_output_high_ and keep a copy
    const bool step_is_now_high = !step_output_high_.load(std::memory_order_relaxed);
    step_output_high_.store(step_is_now_high, std::memory_order_relaxed);

    // Count only on rising edge
    if (step_is_now_high)
    {
        if (direction_ == Direction::clockwise)
            pulse_count_.fetch_add(1U, std::memory_order_relaxed);
        else
            pulse_count_.fetch_sub(1U, std::memory_order_relaxed);
    }

    // If stop is requested, complete the final high pulse before stopping
    if (!step_is_now_high && stop_requested_flag_.load(std::memory_order_relaxed))
    {
        stop_channel_now();
        return;
    }

    // Set next compare value
    const std::uint32_t previous_compare = config_.timer_channel.compare();
    const std::uint32_t half_period = half_period_ticks_.load(std::memory_order_relaxed);
    config_.timer_channel.set_compare(previous_compare + half_period);
}

void StepperPulseGenerator::write_direction(Direction direction) noexcept
{
    HAL_GPIO_WritePin(config_.direction.port, config_.direction.pin,
                      direction == Direction::clockwise
                          ? config_.direction.electrical_state_clockwise
                          : opposite(config_.direction.electrical_state_clockwise));
}