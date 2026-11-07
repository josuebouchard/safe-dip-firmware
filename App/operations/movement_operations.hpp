#ifndef MOVEMENT_OPERATIONS_HPP
#define MOVEMENT_OPERATIONS_HPP

#include "axis.hpp"
#include "operation.hpp"
#include "tick.hpp"
#include <cstdint>

// Moves to a recipe position given in mm, converted through the axis's scale when
// the move starts. Raises the axis's limit_hit if its switch is hit on the way, or
// operation_interrupted if something else cut the move short.
class MoveStepperOperation : public Operation<Tick>
{
  public:
    MoveStepperOperation(const Axis &axis, const std::int32_t &position_mm) noexcept
        : axis_(axis), position_mm_(position_mm)
    {
    }
    // Both are stored by reference, so a temporary would dangle.
    MoveStepperOperation(const Axis &&axis, const std::int32_t &position_mm) = delete;
    MoveStepperOperation(const Axis &axis, const std::int32_t &&position_mm) = delete;

    void start(Tick &ctx) noexcept override;
    PollResult poll(Tick &ctx) noexcept override;

  private:
    const Axis &axis_;
    const std::int32_t &position_mm_;
};

// Searches for the axis's switch at homing speed, sets the origin just past it, and
// backs off to that origin. The switch is expected here, so hitting it isn't a fault.
class HomeStepperOperation : public Operation<Tick>
{
  public:
    HomeStepperOperation(const Axis &axis, std::int32_t back_off_steps,
                         std::uint32_t timeout_ms, std::int32_t drift_tolerance_steps) noexcept
        : axis_{axis}, back_off_steps_{back_off_steps}, timeout_ms_{timeout_ms},
          drift_tolerance_steps_{drift_tolerance_steps}
    {
    }
    // Stored by reference, so a temporary would dangle.
    HomeStepperOperation(const Axis &&axis, std::int32_t back_off_steps,
                         std::uint32_t timeout_ms, std::int32_t drift_tolerance_steps) = delete;

    void start(Tick &ctx) noexcept override;
    PollResult poll(Tick &ctx) noexcept override;

  private:
    const Axis &axis_;
    std::int32_t back_off_steps_;
    std::uint32_t timeout_ms_; // Covers the whole operation: the search and the back-off
    std::int32_t drift_tolerance_steps_;
    bool is_backing_off_ = false;
    std::uint32_t start_time_ = 0;

    // Deliberately not reset in start(): it says whether an earlier run set the
    // origin that this run checks for lost steps.
    bool has_origin_ = false;
};

#endif // MOVEMENT_OPERATIONS_HPP
