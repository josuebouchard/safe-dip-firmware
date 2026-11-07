#ifndef MOVEMENT_OPERATIONS_HPP
#define MOVEMENT_OPERATIONS_HPP

#include "axis_scale.hpp"
#include "operation.hpp"
#include "stepper_controller.hpp"
#include <cstdint>

// Moves to a recipe position given in mm, converted when the move starts.
class MoveStepperOperation : public Operation
{
  public:
    MoveStepperOperation(StepperController &stepper, AxisScale scale,
                         const std::int32_t &position_mm)
        : stepper_(stepper), scale_(scale), position_mm_(position_mm)
    {
    }
    // Stored by reference, so a temporary would dangle.
    MoveStepperOperation(StepperController &stepper, AxisScale scale,
                         const std::int32_t &&position_mm) = delete;

    void start() noexcept override;
    PollResult poll() noexcept override;
    [[nodiscard]] Failure failure() const noexcept override { return Failure::interrupted; }

  private:
    StepperController &stepper_;
    AxisScale scale_;
    const std::int32_t &position_mm_;
};

class HomeStepperOperation : public Operation
{
  public:
    explicit HomeStepperOperation(StepperController &stepper, bool &is_switch_pressed,
                                  std::int32_t back_off_steps, std::uint32_t timeout_ms,
                                  std::int32_t drift_tolerance_steps, Failure homing_failure)
        : stepper_{stepper}, back_off_steps_{back_off_steps}, timeout_ms_{timeout_ms},
          drift_tolerance_steps_{drift_tolerance_steps}, homing_failure_{homing_failure},
          is_switch_pressed_{is_switch_pressed}, is_backing_off_{false}
    {
    }

    void start() noexcept override;
    PollResult poll() noexcept override;
    [[nodiscard]] Failure failure() const noexcept override { return failure_; }

  private:
    StepperController &stepper_;
    std::int32_t back_off_steps_;
    std::uint32_t timeout_ms_; // Covers the whole operation: the search and the back-off
    std::int32_t drift_tolerance_steps_;
    Failure homing_failure_; // x_ or y_homing_failed, for this axis
    bool &is_switch_pressed_;
    bool is_backing_off_;
    std::uint32_t start_time_ = 0;
    Failure failure_ = Failure::unspecified;

    // Deliberately not reset in start(): it says whether an earlier run set the
    // origin that this run checks for lost steps.
    bool has_origin_ = false;
};

#endif // MOVEMENT_OPERATIONS_HPP
