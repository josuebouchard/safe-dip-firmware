#ifndef STEPPER_DIRECTION_PIN_HPP
#define STEPPER_DIRECTION_PIN_HPP

#include "stm32f4xx_hal.h"
#include <cstdint>

class StepperDirectionPin
{
  private:
    GPIO_TypeDef *port;
    std::uint16_t pin;

    // Electrical state corresponding to Direction::clockwise.
    GPIO_PinState electrical_state_clockwise;

    static GPIO_PinState opposite(GPIO_PinState state) noexcept
    {
        return state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

  public:
};

#endif // STEPPER_DIRECTION_PIN_HPP
