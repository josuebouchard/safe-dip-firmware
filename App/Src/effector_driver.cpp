#include "effector_driver.hpp"
#include "stm32f4xx_hal.h"

void EffectorDriver::engage() noexcept { HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_SET); }

void EffectorDriver::disengage() noexcept { HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_RESET); }
