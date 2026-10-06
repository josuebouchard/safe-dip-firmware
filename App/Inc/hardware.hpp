#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include "effector_driver.hpp"
#include "stepper_driver.hpp"

// Board-level driver objects shared by every entrypoint. Pin assignments,
// timer channels and electrical conventions (sinking outputs) live here, so
// entrypoints only hold application logic.
//
// hardware.cpp also owns HAL_TIM_OC_DelayElapsedCallback, which feeds the
// stepper drivers - entrypoints must not define it.
namespace hardware
{

extern StepperDriver stepper_driver_x;
extern StepperDriver stepper_driver_y;

extern EffectorDriver effector;

} // namespace hardware

#endif // HARDWARE_HPP
