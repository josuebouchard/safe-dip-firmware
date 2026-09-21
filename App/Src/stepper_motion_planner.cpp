#include "stepper_motion_planner.hpp"
#include "stepper_driver.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

StepperMotionPlanner::SpeedProfile::SpeedProfile(float max_speed, float acceleration,
                                                 float deceleration, float landing_speed) noexcept
    : max_speed(max_speed), acceleration(acceleration), deceleration(deceleration),
      // At 0 the landing ramp would stall short of the target for good.
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

    // Position mode: run flat out towards the target, then settle at the
    // landing speed once braking has started - process() stops dead on the
    // target step. Heading the wrong way still brakes to zero, and once
    // stopped target_versor() picks the correct direction on the next tick.
    const std::int8_t current_direction = speed_versor();
    if (current_direction != 0 && current_direction != target_versor())
    {
        return 0.0f;
    }

    const float speed = is_landing_ ? speed_profile_.landing_speed : speed_profile_.max_speed;
    return static_cast<float>(target_versor()) * speed;
}

float StepperMotionPlanner::ramp_speed_toward(float speed_desired,
                                              float delta_time_s) const noexcept
{
    const float speed_error = speed_desired - speed_current_signed_;

    if (speed_error == 0.0f)
    {
        return speed_current_signed_;
    }

    // Accelerate only when the magnitude grows without crossing zero.
    // Slowing down and reversing both brake at the deceleration rate; a
    // reversal keeps braking until it passes through zero, after which
    // this test flips and the rest of the move accelerates.
    const bool is_same_direction = (speed_current_signed_ >= 0.0f) == (speed_desired >= 0.0f);
    const bool is_speeding_up =
        is_same_direction && std::abs(speed_desired) > std::abs(speed_current_signed_);

    const float rate = is_speeding_up ? speed_profile_.acceleration : speed_profile_.deceleration;
    const float speed_step = rate * delta_time_s;

    // Snap rather than overshoot - otherwise the speed oscillates around
    // the setpoint once the remaining error is smaller than one step.
    if (std::abs(speed_error) <= speed_step)
    {
        return speed_desired;
    }

    return speed_current_signed_ + std::copysign(speed_step, speed_error);
}

void StepperMotionPlanner::process(std::uint32_t now_ms) noexcept
{
    // @todo: assert(now_ms >= last_ms_)

    // Calculate delta time - integer ms subtraction first (exact, wraparound-safe),
    // then convert only the small delta to float seconds for the speed math.
    const float delta_time_s = static_cast<float>(now_ms - last_ms_) / 1000.0f;
    last_ms_ = now_ms;

    // Stop dead on the target step. Checked ahead of the tick gate below so
    // arrival is caught on every call, before the next pulse goes out.
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

    // HAL_GetTick() only advances every millisecond while loop() runs far
    // faster, so most calls land on the same tick and have nothing to ramp.
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

    // Round rather than truncate: a commanded 0.6 steps/s would otherwise
    // reach the driver as 0 and disarm the channel mid-ramp.
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
    // Clear both setpoints, or the next process() ramps straight back up
    // towards whatever was commanded before the stop.
    speed_target_signed_ = 0.0f;
    position_target_signed_ = position_current_signed();
    is_landing_ = false;

    stepper_driver_.stop_immediately();
}

void StepperMotionPlanner::reset_position(std::int32_t position) noexcept
{
    // Stop first: the ISR writes pulse_count_ on every active edge, so
    // resetting it while the channel is armed can lose an in-flight step.
    stop_immediately();

    stepper_driver_.reset_pulse_count(position);

    // Move the target with the origin - leaving it stale would make the
    // next position-mode tick drive towards the old target.
    position_target_signed_ = position;
}
