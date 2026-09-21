#include "stepper_motion_planner.hpp"
#include "stepper_driver.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

template <typename T> static constexpr std::int8_t sign(T value) noexcept
{
    return (value > 0) - (value < 0);
}

StepperMotionPlanner::SpeedProfile::SpeedProfile(float max_speed, float acceleration,
                                                 float deceleration, float landing_speed) noexcept
    : max_speed(max_speed), acceleration(acceleration), deceleration(deceleration),
      // A zero landing speed would stall short of the target.
      landing_speed(std::min(std::max(landing_speed, 1.0f), max_speed))
{
}

std::int32_t StepperMotionPlanner::position_current_signed() const noexcept
{
    return stepper_driver_.pulse_count();
}

bool StepperMotionPlanner::is_at_target() const noexcept
{
    return position_current_signed() == position_target_signed_;
}

bool StepperMotionPlanner::is_at_target_speed() const noexcept
{
    return speed_current_signed_ == speed_target_signed_;
}

std::int8_t StepperMotionPlanner::speed_versor() const noexcept
{
    return sign(speed_current_signed_);
}

std::int8_t StepperMotionPlanner::target_versor() const noexcept
{
    const int64_t distance = static_cast<int64_t>(position_target_signed_) -
                             static_cast<int64_t>(position_current_signed());

    return sign(distance);
}

// @todo: assert(speed_versor() == target_versor()) - process() gates on it
bool StepperMotionPlanner::should_decelerate() const noexcept
{
    const int64_t distance = static_cast<int64_t>(position_target_signed_) -
                             static_cast<int64_t>(position_current_signed());

    const float speed_magnitude = std::abs(speed_current_signed_);
    const float landing_speed = speed_profile_.landing_speed;

    // Distance to slow down to the landing speed, not to a standstill.
    const float braking_distance =
        std::max(0.0f, speed_magnitude * speed_magnitude - landing_speed * landing_speed) /
        (2.0f * speed_profile_.deceleration);

    return std::abs(static_cast<float>(distance)) <= braking_distance;
}

float StepperMotionPlanner::speed_desired_signed() const noexcept
{
    if (mode_ == ControlMode::velocity)
    {
        return speed_target_signed_;
    }

    // Position mode: max speed towards the target, landing speed once
    // braking starts. Heading the wrong way brakes to zero first.
    const std::int8_t current_direction = speed_versor();
    const std::int8_t target_direction = target_versor();
    if (current_direction != 0 && current_direction != target_direction)
    {
        return 0.0f;
    }

    const float speed = is_landing_ ? speed_profile_.landing_speed : speed_profile_.max_speed;
    return static_cast<float>(target_direction) * speed;
}

float StepperMotionPlanner::ramp_speed_toward(float speed_desired,
                                              float delta_time_s) const noexcept
{
    const float speed_error = speed_desired - speed_current_signed_;

    // Accelerate only when the magnitude grows without crossing zero;
    // slowing down and reversing both use the deceleration rate.
    const bool is_same_direction = (speed_current_signed_ >= 0.0f) == (speed_desired >= 0.0f);
    const bool is_speeding_up =
        is_same_direction && std::abs(speed_desired) > std::abs(speed_current_signed_);

    const float rate = is_speeding_up ? speed_profile_.acceleration : speed_profile_.deceleration;
    const float speed_step = rate * delta_time_s;

    // Snap to the setpoint instead of oscillating around it.
    if (std::abs(speed_error) <= speed_step)
    {
        return speed_desired;
    }

    return speed_current_signed_ + std::copysign(speed_step, speed_error);
}

void StepperMotionPlanner::process(std::uint32_t now_ms) noexcept
{
    // @todo: assert(now_ms >= last_ms_)

    // Integer ms subtraction first (wraparound-safe), then convert the small delta.
    const float delta_time_s = static_cast<float>(now_ms - last_ms_) / 1000.0f;
    last_ms_ = now_ms;

    // Stop dead on the target step. Ahead of the tick gate so arrival is
    // caught on every call.
    if (mode_ == ControlMode::position && is_at_target())
    {
        is_landing_ = false;

        if (speed_current_signed_ != 0.0f)
        {
            speed_current_signed_ = 0.0f;
            stepper_driver_.set_speed(0);
        }
        return;
    }

    // HAL_GetTick() advances once per ms, so most calls have nothing to ramp.
    if (delta_time_s == 0.0f)
    {
        return;
    }

    if (mode_ == ControlMode::position && speed_versor() == target_versor() && should_decelerate())
    {
        is_landing_ = true;
    }

    const float speed_desired = speed_desired_signed();

    // Idle - standing still with nothing commanded.
    if (speed_current_signed_ == 0.0f && speed_desired == 0.0f)
    {
        return;
    }

    speed_current_signed_ = std::clamp(ramp_speed_toward(speed_desired, delta_time_s),
                                       -speed_profile_.max_speed, speed_profile_.max_speed);

    // Round, not truncate: 0.6 steps/s would reach the driver as 0 and disarm it.
    stepper_driver_.set_speed(static_cast<std::int32_t>(std::lround(speed_current_signed_)));
}

void StepperMotionPlanner::move_to_steps(std::int32_t target_steps) noexcept
{
    mode_ = ControlMode::position;
    position_target_signed_ = target_steps;
    is_landing_ = false;
}

void StepperMotionPlanner::move_at_speed(float steps_per_second) noexcept
{
    mode_ = ControlMode::velocity;
    is_landing_ = false;
    speed_target_signed_ =
        std::clamp(steps_per_second, -speed_profile_.max_speed, speed_profile_.max_speed);
}

void StepperMotionPlanner::stop() noexcept
{
    mode_ = ControlMode::velocity;
    speed_target_signed_ = 0.0f;
    is_landing_ = false;
}

void StepperMotionPlanner::stop_immediately() noexcept
{
    speed_current_signed_ = 0.0f;
    // Clear both setpoints, or the next process() ramps back up.
    speed_target_signed_ = 0.0f;
    position_target_signed_ = position_current_signed();
    is_landing_ = false;

    stepper_driver_.stop_immediately();
}

void StepperMotionPlanner::reset_position(std::int32_t position) noexcept
{
    // Stop first: the ISR writes pulse_count_ on every active edge.
    stop_immediately();

    stepper_driver_.reset_pulse_count(position);

    // Move the target with the origin, or the next tick drives to the old one.
    position_target_signed_ = position;
}
