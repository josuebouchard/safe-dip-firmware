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
        float landing_speed;
    };

    explicit StepperMotionPlanner(const SpeedProfile &speed_profile, StepperDriver &pulse_generator)
        : speed_profile_(speed_profile), stepper_driver_(pulse_generator), last_ms_(0)
    {
    }

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

    std::int32_t position_current_signed() const noexcept;

    // False in velocity mode - there is no position target to be at.
    bool is_at_target() const noexcept;

  private:
    const SpeedProfile &speed_profile_;
    StepperDriver &stepper_driver_;

    enum class Phase : std::uint8_t
    {
        velocity,         // Aims for the commanded speed
        position,         // Aims for `max_speed` towards target
        position_landing, // Aims for `landing_speed` towards target
    };
    Phase phase_ = Phase::position;

    // Both position phases chase a step target; they differ only in which speed they aim for.
    bool is_position_mode() const noexcept { return phase_ != Phase::velocity; }

    // Target
    std::int32_t position_target_signed_ = 0;
    float speed_target_signed_ = 0.0f; // steps / second

    // Current
    float speed_current_signed_ = 0.0f; // steps / second

    // Last
    std::uint32_t last_ms_;

    // Helpers
    std::int64_t distance_to_target() const noexcept;

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
