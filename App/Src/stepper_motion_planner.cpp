#include "stepper_motion_planner.hpp"
#include "stepper_driver.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

std::int32_t StepperMotionPlanner::position_current_signed() const noexcept
{
    return stepper_driver_.pulse_count();
}

bool StepperMotionPlanner::is_at_target() const noexcept
{
    return position_current_signed() == position_target_signed_;
}

std::int8_t StepperMotionPlanner::speed_versor() const noexcept
{
    if (speed_current_signed_ > 0)
        return 1;
    else if (speed_current_signed_ < 0)
        return -1;
    else
        return 0;
}

std::int8_t StepperMotionPlanner::target_versor() const noexcept
{
    const int64_t distance = static_cast<int64_t>(position_target_signed_) -
                             static_cast<int64_t>(position_current_signed());

    return (distance > 0) - (distance < 0);
}

bool StepperMotionPlanner::should_decelerate() const noexcept
{
    const int32_t position = position_current_signed();
    const int64_t distance =
        static_cast<int64_t>(position_target_signed_) - static_cast<int64_t>(position);

    const int8_t target_direction = (distance > 0) - (distance < 0);

    const int8_t current_direction = speed_versor();

    if (current_direction == 0)
    {
        return false;
    }

    // Already moving away from the target:
    // brake before trying to reverse.
    if (current_direction != target_direction)
    {
        return true;
    }

    const double speed_magnitude = std::abs(speed_current_signed_);

    const double braking_distance =
        (speed_magnitude * speed_magnitude) / (2.0 * speed_profile_.deceleration);

    return std::abs(static_cast<double>(distance)) <= braking_distance;
}

void StepperMotionPlanner::process(std::uint32_t now_ms) noexcept
{
    // @todo: assert(now_ms >= last_ms_)

    // Calculate delta time
    const auto delta_time = now_ms - last_ms_;
    last_ms_ = now_ms;

    bool is_stopped = std::abs(speed_current_signed_) == 0;

    if (is_stopped && is_at_target())
        return; // No-op - should be idle

    const auto speed_current_magnitude = std::abs(speed_current_signed_);
    double acceleration;
    int8_t direction;
    if (should_decelerate())
    {
        acceleration = -speed_profile_.deceleration;
        direction = speed_versor();
    }
    else
    {
        acceleration = speed_profile_.acceleration;
        direction = target_versor();
    }

    auto speed_new_magnitude =
        std::clamp(speed_current_magnitude + acceleration * static_cast<double>(delta_time), 0.0,
                   speed_profile_.max_speed);

    // Apply speed
    speed_current_signed_ = speed_new_magnitude * direction;

    stepper_driver_.set_speed(speed_current_signed_);
}

void StepperMotionPlanner::move_to_steps(std::int32_t target_microsteps) noexcept
{
    position_target_signed_ = target_microsteps;
}

void StepperMotionPlanner::move_continously(std::int32_t direction) noexcept
{
    if (direction > 0)
        position_target_signed_ = std::numeric_limits<int32_t>::max();
    else if (direction < 0)
        position_target_signed_ = std::numeric_limits<int32_t>::min();
    else
        stop_immediately();
}

void StepperMotionPlanner::stop_immediately() noexcept
{
    speed_current_signed_ = 0.0;
    position_target_signed_ = position_current_signed();

    stepper_driver_.stop_immediately();
}
