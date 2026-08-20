#ifndef MOVEMENT_OPERATIONS_HPP
#define MOVEMENT_OPERATIONS_HPP

#include "operation.hpp"
#include "stepper_motion_planner.hpp"
#include <cstdint>

class MoveStepperOperation : public Operation
{
  public:
    MoveStepperOperation(StepperMotionPlanner &stepper, std::int32_t position)
        : stepper_(stepper), position_(position)
    {
    }

    void start() noexcept override;
    PollResult poll() noexcept override;
    void cancel() noexcept override;

    void set_position(std::int32_t position) noexcept { position_ = position; }
    std::int32_t position() const noexcept { return position_; }

  private:
    StepperMotionPlanner &stepper_;
    std::int32_t position_;
};

#endif // MOVEMENT_OPERATIONS_HPP
