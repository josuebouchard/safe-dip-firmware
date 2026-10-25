#ifndef STEPPER_MOTION_PLANNER_HPP
#define STEPPER_MOTION_PLANNER_HPP

#include "stepper_driver.hpp"
#include <cstdint>
#include <variant>

class StepperMotionPlanner
{
  public:
    struct SpeedProfile
    {
        SpeedProfile(float max_speed, float acceleration, float landing_speed,
                     float homing_speed) noexcept;

        float max_speed;
        float acceleration;
        float landing_speed;
        float homing_speed;
    };

    explicit StepperMotionPlanner(const SpeedProfile &speed_profile, StepperDriver &pulse_generator)
        : speed_profile_(speed_profile), stepper_driver_(pulse_generator), last_ms_(0)
    {
    }

    void initialize(std::uint32_t now_ms) { last_ms_ = now_ms; }

    // Movement interface
    void move_to_steps(std::int32_t target_steps) noexcept;
    void move_steps(std::int32_t delta_steps) noexcept;
    void move_at_speed(float steps_per_second) noexcept;
    void move_at_speed_in_direction(float direction) noexcept;

    // Ramps down at the acceleration rate.
    void stop() noexcept;
    // Cuts the pulse train instantly, no ramp.
    void stop_immediately() noexcept;

    // Redefines the current position - call after homing, while stopped.
    void reset_position(std::int32_t position = 0) noexcept;

    void process(std::uint32_t now_ms) noexcept;

    std::int32_t position_current_signed() const noexcept;

    // False in velocity mode - there is no position target to be at.
    bool is_at_target() const noexcept;

    float current_speed_signed() const noexcept;

  private:
    const SpeedProfile &speed_profile_;
    StepperDriver &stepper_driver_;

    // No default member initializers: they'd make these not default-constructible
    // inside the enclosing class, which std::variant needs. Aggregate init zero-fills.
    struct PositionMode
    {
        std::int32_t position_target_signed;
        // Aims for `landing_speed` instead of `max_speed` once braking starts.
        bool is_landing;
    };

    struct VelocityMode
    {
        float speed_target_signed; // steps / second
    };

    std::variant<PositionMode, VelocityMode> mode_;

    // Current
    float speed_current_signed_ = 0.0f; // steps / second

    // Last
    std::uint32_t last_ms_;

    // Helpers
    std::int64_t distance_to_target(const PositionMode &position) const noexcept;

    std::int8_t speed_versor() const noexcept;
    std::int8_t target_versor(const PositionMode &position) const noexcept;

    bool should_decelerate(const PositionMode &position) const noexcept;

    // The speed process() aims for this tick in position mode.
    float speed_desired_signed(const PositionMode &position) const noexcept;

    // Single place where speed changes over time - both modes go through it.
    float ramp_speed_toward(float speed_desired, float delta_time_s) const noexcept;
};

#endif // STEPPER_MOTION_PLANNER_HPP
