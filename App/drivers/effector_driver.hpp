#ifndef EFFECTOR_DRIVER_HPP
#define EFFECTOR_DRIVER_HPP

#include "stm32f4xx_hal.h"
#include <cstdint>

class EffectorDriver
{
  public:
    EffectorDriver(GPIO_TypeDef *port, std::uint16_t pin) noexcept : port_{port}, pin_{pin} {}

    void engage() noexcept;
    void disengage() noexcept;
    bool is_engaged() const noexcept;

  private:
    GPIO_TypeDef *port_;
    std::uint16_t pin_;
};

#endif // EFFECTOR_DRIVER_HPP
