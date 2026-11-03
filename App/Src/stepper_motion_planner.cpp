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
                                                 float landing_speed, float homing_speed) noexcept
    : max_speed(max_speed), acceleration(acceleration),
      // A zero landing speed would stall short of the target.
      landing_speed(std::min(std::max(landing_speed, 1.0f), max_speed)),
      // Same bounds: a zero homing speed would never reach the switch.
      homing_speed(std::min(std::max(homing_speed, 1.0f), max_speed))
{
}

std::int32_t StepperMotionPlanner::position_current_signed() const noexcept
{
    return stepper_driver_.pulse_count();
}

bool StepperMotionPlanner::is_at_target() const noexcept
{
    const auto *position = std::get_if<PositionMode>(&mode_);
    return position && distance_to_target(*position) == 0;
}

float StepperMotionPlanner::current_speed_signed() const noexcept { return speed_current_signed_; }

std::int8_t StepperMotionPlanner::speed_versor() const noexcept
{
    return sign(speed_current_signed_);
}

std::int64_t StepperMotionPlanner::distance_to_target(const PositionMode &position) const noexcept
{
    return static_cast<std::int64_t>(position.position_target_signed) -
           static_cast<std::int64_t>(position_current_signed());
}

std::int8_t StepperMotionPlanner::target_versor(const PositionMode &position) const noexcept
{
    return sign(distance_to_target(position));
}

// Only meaningful once already heading toward the target - process() gates on it.
bool StepperMotionPlanner::should_decelerate(const PositionMode &position) const noexcept
{
    const std::int64_t distance = distance_to_target(position);

    const float speed_magnitude = std::abs(speed_current_signed_);
    const float landing_speed = speed_profile_.landing_speed;

    // Distance to slow down to the landing speed, not to a standstill.
    const float braking_distance =
        std::max(0.0f, speed_magnitude * speed_magnitude - landing_speed * landing_speed) /
        (2.0f * speed_profile_.acceleration);

    return std::abs(static_cast<float>(distance)) <= braking_distance;
}

// Max speed towards the target, landing speed once braking starts.
// Heading the wrong way brakes to zero first.
float StepperMotionPlanner::speed_desired_signed(const PositionMode &position) const noexcept
{
    const std::int8_t current_direction = speed_versor();
    const std::int8_t target_direction = target_versor(position);

    // @todo: assert(target_direction != 0)

    // process() gates on !is_at_target()
    if (current_direction == -target_direction)
    {
        return 0.0f;
    }

    const float speed =
        position.is_landing ? speed_profile_.landing_speed : speed_profile_.max_speed;
    return static_cast<float>(target_direction) * speed;
}

float StepperMotionPlanner::ramp_speed_toward(float speed_desired,
                                              float delta_time_s) const noexcept
{
    const float speed_error = speed_desired - speed_current_signed_;

    const float speed_step = speed_profile_.acceleration * delta_time_s;

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

    auto *position = std::get_if<PositionMode>(&mode_);

    // Stop dead on the target step. Above the tick gate because this tests
    // exact equality - checking only once per tick could let a fast move
    // step past the target value without ever matching it.
    if (position && distance_to_target(*position) == 0)
    {
        // Landing latch, half 1 of 2: arrival clears it.
        position->is_landing = false;

        if (speed_current_signed_ != 0.0f)
        {
            stop_immediately();
        }
        return;
    }

    // HAL_GetTick() advances once per ms, so most calls have nothing to ramp.
    if (delta_time_s == 0.0f)
    {
        return;
    }

    // Landing latch, half 2 of 2: braking sets it. Below the tick gate because
    // should_decelerate() cannot change within a millisecond, and the superloop
    // calls process() far more often than that.
    if (position && speed_versor() == target_versor(*position) && should_decelerate(*position))
    {
        position->is_landing = true;
    }

    // Not std::visit: it pulls ~7 KB of vfprintf into flash.
    const auto *velocity = std::get_if<VelocityMode>(&mode_);
    const float speed_desired =
        velocity ? velocity->speed_target_signed : speed_desired_signed(*position);

    // Idle - standing still with nothing commanded.
    if (speed_current_signed_ == 0.0f && speed_desired == 0.0f)
    {
        return;
    }

    // Already bounded: ramp_speed_toward() never overshoots its setpoint, and both
    // endpoints are within the profile, which is const for the planner's lifetime.
    speed_current_signed_ = ramp_speed_toward(speed_desired, delta_time_s);

    // Round, not truncate: 0.6 steps/s would reach the driver as 0 and disarm it.
    stepper_driver_.set_speed(static_cast<std::int32_t>(std::lround(speed_current_signed_)));
}

void StepperMotionPlanner::move_to_steps(std::int32_t target_steps) noexcept
{
    was_interrupted_ = false;
    mode_ = PositionMode{
        .position_target_signed = target_steps,
        .is_landing = false,
    };
}

void StepperMotionPlanner::move_steps(std::int32_t delta_steps) noexcept
{
    move_to_steps(position_current_signed() + delta_steps);
}

void StepperMotionPlanner::move_at_speed(float steps_per_second) noexcept
{
    was_interrupted_ = false;
    mode_ = VelocityMode{
        .speed_target_signed =
            std::clamp(steps_per_second, -speed_profile_.max_speed, speed_profile_.max_speed),
    };
}

void StepperMotionPlanner::move_at_speed_in_direction(float direction) noexcept
{
    move_at_speed(std::copysignf(speed_profile_.homing_speed, direction));
}

void StepperMotionPlanner::stop() noexcept
{
    // A zero velocity target ramps down gracefully from any mode.
    mode_ = VelocityMode{
        .speed_target_signed = 0.0f,
    };
}

void StepperMotionPlanner::stop_immediately() noexcept
{
    // No deceleration
    speed_current_signed_ = 0.0f;

    // If current mode is position, get a pointer to it
    const auto *position = std::get_if<PositionMode>(&mode_);

    // process() also calls this on normal arrival, so only a stop with distance
    // remaining counts as an interruption.
    // This call moves the target to the current position, so
    // a repeat call sees zero distance and must not clear the flag.
    if (position && distance_to_target(*position) != 0)
    {
        was_interrupted_ = true;
    }

    // Only neutralize the setpoint of the active mode
    if (position)
    {
        mode_ = PositionMode{
            .position_target_signed = position_current_signed(),
            .is_landing = false,
        };
    }
    else
    {
        mode_ = VelocityMode{
            .speed_target_signed = 0.0f,
        };
    }

    // Cut the OC timer interrupt
    stepper_driver_.stop_immediately();
}

void StepperMotionPlanner::reset_position(std::int32_t position) noexcept
{
    // Stop first: the ISR writes pulse_count_ on every active edge.
    stop_immediately();

    stepper_driver_.reset_pulse_count(position);

    // Move the target with the origin, or the next tick drives to the old one.
    if (std::holds_alternative<PositionMode>(mode_))
    {
        mode_ = PositionMode{
            .position_target_signed = position,
            .is_landing = false,
        };
    }
}
