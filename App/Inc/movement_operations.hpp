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

  private:
    StepperController &stepper_;
    const std::int32_t &position_;
};

class HomeStepperOperation : public Operation
{
  public:
    explicit HomeStepperOperation(StepperController &stepper, bool &is_switch_pressed,
                                  std::int16_t back_off_steps)
        : stepper_{stepper}, back_off_steps_{back_off_steps}, is_switch_pressed_{is_switch_pressed},
          is_backing_off_{false}
    {
    }

    void start() noexcept override;
    PollResult poll() noexcept override;

  private:
    StepperController &stepper_;
    std::int16_t back_off_steps_;
    bool &is_switch_pressed_;
    bool is_backing_off_;
};

#endif // MOVEMENT_OPERATIONS_HPP
