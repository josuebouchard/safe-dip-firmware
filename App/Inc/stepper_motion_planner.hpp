#ifndef STEPPER_MOTION_PLANNER_HPP
#define STEPPER_MOTION_PLANNER_HPP

#include "stepper_driver.hpp"
#include <cstdint>

class StepperMotionPlanner
{
  public:
    struct SpeedProfile
    {
        SpeedProfile(float max_speed, float acceleration, float deceleration,
                     float landing_speed) noexcept;

        float max_speed;
        float acceleration;
        float deceleration;
        // Braking settles at this speed, then the axis stops dead on the target step.
        float landing_speed;
    };

    explicit StepperMotionPlanner(SpeedProfile& speed_profile, StepperDriver &pulse_generator)
        : stepper_driver_(pulse_generator), last_ms_(0), speed_profile_(speed_profile) {}

    // Which setpoint process() drives towards. Position mode chases a step
    // target and brakes on arrival; velocity mode holds a speed and never
    // reads the position at all.
    enum class ControlMode : std::uint8_t
    {
        position,
        velocity,
    };

    void initialize(std::uint32_t now_ms) { last_ms_ = now_ms; }

    // Movement interface
    void move_to_steps(std::int32_t target_steps) noexcept;
    void move_at_speed(float steps_per_second) noexcept;

    // Ramps down at the deceleration rate.
    void stop() noexcept;
    // Cuts the pulse train instantly, no ramp.
    void stop_immediately() noexcept;

    // Redefines the current position - call after homing, while stopped.
    void reset_position(std::int32_t position = 0) noexcept;

    void process(std::uint32_t now_ms) noexcept;

    const SpeedProfile &speed_profile() const noexcept { return speed_profile_; }
    ControlMode control_mode() const noexcept { return mode_; }
    std::int32_t position_current_signed() const noexcept;

    // Position mode only - there is no position target in velocity mode.
    bool is_at_target() const noexcept;
    bool is_at_target_speed() const noexcept;

  private:
    StepperDriver &stepper_driver_;

    ControlMode mode_ = ControlMode::position;

    // Latched once braking starts, so the ramp never re-accelerates on approach.
    bool is_landing_ = false;

    // Target
    std::int32_t position_target_signed_ = 0;
    float speed_target_signed_ = 0.0f; // steps / second

    // Current
    float speed_current_signed_ = 0.0f; // steps / second

    // Last
    std::uint32_t last_ms_;

    SpeedProfile& speed_profile_;

    // Helpers
    std::int8_t speed_versor() const noexcept;
    std::int8_t target_versor() const noexcept;

    bool should_decelerate() const noexcept;

    // The speed process() aims for this tick, derived from whichever
    // setpoint the active mode uses.
    float speed_desired_signed() const noexcept;

    // Single place where speed changes over time - both modes go through it.
    float ramp_speed_toward(float speed_desired, float delta_time_s) const noexcept;
};

#endif // STEPPER_MOTION_PLANNER_HPP
