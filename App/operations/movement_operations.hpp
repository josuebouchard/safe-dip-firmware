#ifndef MOVEMENT_OPERATIONS_HPP
#define MOVEMENT_OPERATIONS_HPP

#include "operation.hpp"
#include "stepper_controller.hpp"
#include <cstdint>

class MoveStepperOperation : public Operation
{
  public:
    MoveStepperOperation(StepperController &stepper, const std::int32_t &position)
        : stepper_(stepper), position_(position)
    {
    }
    // Stored by reference, so a temporary would dangle.
    MoveStepperOperation(StepperController &stepper, const std::int32_t &&position) = delete;

    void start() noexcept override;
    PollResult poll() noexcept override;
    Failure failure() const noexcept override { return Failure::interrupted; }

  private:
    StepperController &stepper_;
    const std::int32_t &position_;
};

class HomeStepperOperation : public Operation
{
  public:
    explicit HomeStepperOperation(StepperController &stepper, bool &is_switch_pressed,
                                  std::int16_t back_off_steps, std::uint32_t timeout_ms,
                                  std::int32_t drift_tolerance_steps)
        : stepper_{stepper}, back_off_steps_{back_off_steps}, timeout_ms_{timeout_ms},
          drift_tolerance_steps_{drift_tolerance_steps}, is_switch_pressed_{is_switch_pressed},
          is_backing_off_{false}
    {
    }

    void start() noexcept override;
    PollResult poll() noexcept override;
    Failure failure() const noexcept override { return failure_; }

  private:
    StepperController &stepper_;
    std::int16_t back_off_steps_;
    std::uint32_t timeout_ms_; // Covers the whole operation: the search and the back-off
    std::int32_t drift_tolerance_steps_;
    bool &is_switch_pressed_;
    bool is_backing_off_;
    std::uint32_t start_time_ = 0;
    Failure failure_ = Failure::unspecified;

    // Deliberately not reset in start(): it says whether an earlier run set the
    // origin that this run checks for lost steps.
    bool has_origin_ = false;
};

#endif // MOVEMENT_OPERATIONS_HPP
