#include "stm32_output_compare_channel.hpp"

void STM32OutputCompareChannel::start_interrupt(std::uint32_t initial_offset_ticks) noexcept
{
    // Schedule relative to the current counter - it's free-running and may
    // already be far past 0, so a fixed Pulse would wait for a full wrap.
    TIM_OC_InitTypeDef oc = {
        .OCMode = TIM_OCMODE_TOGGLE,
        .Pulse = counter() + initial_offset_ticks,
        .OCPolarity = config_.polarity,
        .OCFastMode = TIM_OCFAST_DISABLE,
    };

    HAL_TIM_OC_ConfigChannel(config_.timer, &oc, config_.timer_channel);

    // Start CC interrupt
    HAL_TIM_OC_Start_IT(config_.timer, config_.timer_channel);
}

void STM32OutputCompareChannel::stop_interrupt() noexcept
{
    HAL_TIM_OC_Stop_IT(config_.timer, config_.timer_channel);

    // Reconfigure the OC reference to inactive (logical low).
    TIM_OC_InitTypeDef oc = {
        .OCMode = TIM_OCMODE_FORCED_INACTIVE,
        .Pulse = 0,
        .OCPolarity = config_.polarity,
        .OCFastMode = TIM_OCFAST_DISABLE,
    };

    HAL_TIM_OC_ConfigChannel(config_.timer, &oc, config_.timer_channel);
}

std::uint32_t STM32OutputCompareChannel::counter() const noexcept
{
    return __HAL_TIM_GET_COUNTER(config_.timer);
}

std::uint32_t STM32OutputCompareChannel::compare() const noexcept
{
    return __HAL_TIM_GET_COMPARE(config_.timer, config_.timer_channel);
}

void STM32OutputCompareChannel::set_compare(std::uint32_t value) noexcept
{
    switch (config_.timer_channel)
    {
    case TIM_CHANNEL_1:
        config_.timer->Instance->CCR1 = value;
        return;
    case TIM_CHANNEL_2:
        config_.timer->Instance->CCR2 = value;
        return;
    case TIM_CHANNEL_3:
        config_.timer->Instance->CCR3 = value;
        return;
    case TIM_CHANNEL_4:
        config_.timer->Instance->CCR4 = value;
        return;
    default:
        return; // @todo: panic
    }
}
