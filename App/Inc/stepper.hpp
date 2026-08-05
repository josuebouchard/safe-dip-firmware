#ifndef STEPPER_HPP
#define STEPPER_HPP

#include <cstdint>

enum class MotionStatus
{
    idle,
    running,
    finished,
    aborted,
};

struct SpeedProfile
{
    double max_speed;
    double acceleration;
    double deceleration;
};

class Stepper
{
    // Target
    std::int32_t position_target_signed_ = 0;

    // Current
    std::int32_t position_current_signed_ = 0;
    double speed_current_signed_ = 0; // microsteps / ms

    // Last
    std::uint32_t last_ms_;

    SpeedProfile speed_profile_;

    void update_speed(double acceleration_signed, std::uint32_t delta_time) noexcept;

    // Helpers
    std::int8_t speed_versor() const noexcept;
    std::int8_t target_versor() const noexcept;

    bool should_decelerate() const noexcept;

  public:
    explicit Stepper(SpeedProfile speed_profile, std::uint32_t now_ms)
        : last_ms_(now_ms), speed_profile_(speed_profile) {};

    // Movement interface
    void move_to_steps(std::int32_t target_microsteps) noexcept;
    void process(std::uint32_t now_ms) noexcept;

    // Configuration interface
    void set_speed_profile(SpeedProfile speed_profile) noexcept;

    const SpeedProfile &speed_profile() const noexcept { return speed_profile_; }
};

#endif // STEPPER_HPP
