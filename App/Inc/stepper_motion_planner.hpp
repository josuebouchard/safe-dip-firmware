#ifndef STEPPER_MOTION_PLANNER_HPP
#define STEPPER_MOTION_PLANNER_HPP

#include "stepper_driver.hpp"
#include <cstdint>

class StepperMotionPlanner
{
  public:
    struct SpeedProfile
    {
        double max_speed;
        double acceleration;
        double deceleration;
    };

    explicit StepperMotionPlanner(SpeedProfile speed_profile, StepperDriver &pulse_generator,
                                  std::uint32_t now_ms)
        : stepper_driver_(pulse_generator), last_ms_(now_ms), speed_profile_(speed_profile) {};

    // Movement interface
    void move_to_steps(std::int32_t target_microsteps) noexcept;
    void move_continously(std::int32_t direction) noexcept;
    void stop_immediately() noexcept;

    void process(std::uint32_t now_ms) noexcept;

    // Configuration interface
    void set_speed_profile(SpeedProfile speed_profile) noexcept;

    const SpeedProfile &speed_profile() const noexcept { return speed_profile_; }
    std::int32_t position_current_signed() const noexcept;
    bool is_at_target() const noexcept;

  private:
    StepperDriver &stepper_driver_;
    // Target
    std::int32_t position_target_signed_ = 0;

    // Current
    double speed_current_signed_ = 0; // microsteps / ms

    // Last
    std::uint32_t last_ms_;

    SpeedProfile speed_profile_;

    void update_speed(double acceleration_signed, std::uint32_t delta_time) noexcept;

    // Helpers
    std::int8_t speed_versor() const noexcept;
    std::int8_t target_versor() const noexcept;

    bool should_decelerate() const noexcept;
};

#endif // STEPPER_MOTION_PLANNER_HPP
