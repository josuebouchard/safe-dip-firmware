#include "effector_driver.hpp"
#include "stm32f4xx_hal.h"

void EffectorDriver::engage() noexcept { HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_SET); }

void EffectorDriver::disengage() noexcept { HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_RESET); }

// Commanded state from the output latch; there is no feedback from the magnet.
bool EffectorDriver::is_engaged() const noexcept { return (port_->ODR & pin_) != 0U; }
