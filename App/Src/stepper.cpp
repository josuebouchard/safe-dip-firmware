#include "stepper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

std::int8_t Stepper::speed_versor() const noexcept
{
    if (speed_current_signed_ > 0)
        return 1;
    else if (speed_current_signed_ < 0)
        return -1;
    else
        return 0;
}

std::int8_t Stepper::target_versor() const noexcept
{
    if (position_target_signed_ > position_current_signed_)
        return 1;
    else if (position_target_signed_ < position_current_signed_)
        return -1;
    else
        return 0;
}

bool Stepper::should_decelerate() const noexcept
{
    // If no speed yet not at the target, shouldn't decelerate
    if (speed_versor() == 0)
        return false;

    // Moving away from the target
    if (speed_versor() != target_versor())
        return true;

    // Check if in braking zone

    const auto deceleration_signed = -1 * speed_profile_.deceleration * speed_versor();

    const auto braking_start_position =
        (speed_current_signed_ * speed_current_signed_) / (2.0 * deceleration_signed) +
        position_target_signed_;

    return (static_cast<double>(position_current_signed_) - braking_start_position) *
               target_versor() >=
           0;
}

void Stepper::process(std::uint32_t now_ms) noexcept
{
    // @todo: assert(now_ms >= last_ms_)

    // Calculate delta time
    const auto delta_time = now_ms - last_ms_;
    last_ms_ = now_ms;

    bool is_stopped = std::abs(speed_current_signed_) == 0;
    bool is_at_target = position_current_signed_ == position_target_signed_;

    if (is_stopped && is_at_target)
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
}

void Stepper::move_to_steps(std::int32_t target_microsteps) noexcept
{
    position_target_signed_ = target_microsteps;
}
