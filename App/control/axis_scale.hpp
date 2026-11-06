#ifndef AXIS_SCALE_HPP
#define AXIS_SCALE_HPP

#include <cstdint>

// Converts distances along an axis, in mm, to motor steps.
class AxisScale
{
  public:
    // `pulses_per_rev` is what the driver is set to: full steps times microsteps.
    static constexpr AxisScale leadscrew(std::int32_t pulses_per_rev, std::int32_t lead_mm) noexcept
    {
        // The lead, not the pitch: a multi-start screw advances pitch x starts per rev.
        return AxisScale{pulses_per_rev, lead_mm};
    }

    static constexpr AxisScale pulley(std::int32_t pulses_per_rev, std::int32_t teeth,
                                      std::int32_t belt_pitch_mm) noexcept
    {
        return AxisScale{pulses_per_rev, teeth * belt_pitch_mm};
    }

    // Rounded to the nearest step.
    [[nodiscard]] constexpr std::int32_t steps_from_mm(std::int32_t mm) const noexcept
    {
        // 64-bit, so a long travel at a high microstep setting can't overflow.
        return static_cast<std::int32_t>(
            divide_rounded(std::int64_t{mm} * pulses_per_rev_, mm_per_rev_));
    }

    // For speeds and accelerations: multiply mm/s or mm/s^2 by this.
    [[nodiscard]] constexpr float steps_per_mm() const noexcept
    {
        return static_cast<float>(pulses_per_rev_) / static_cast<float>(mm_per_rev_);
    }

  private:
    constexpr AxisScale(std::int32_t pulses_per_rev, std::int32_t mm_per_rev) noexcept
        : pulses_per_rev_{pulses_per_rev}, mm_per_rev_{mm_per_rev}
    {
    }

    // Half away from zero. `denominator` is positive.
    static constexpr std::int64_t divide_rounded(std::int64_t numerator,
                                                 std::int64_t denominator) noexcept
    {
        const std::int64_t half = denominator / 2;
        return numerator >= 0 ? (numerator + half) / denominator
                              : -((-numerator + half) / denominator);
    }

    std::int32_t pulses_per_rev_;
    std::int32_t mm_per_rev_;
};

#endif // AXIS_SCALE_HPP
