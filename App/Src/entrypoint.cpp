#include "entrypoint.hpp"

#include "effector_operations.hpp"
#include "ethernet_if.h"
#include "hardware.hpp"
#include "modbus_tcp_task.h"
#include "movement_operations.hpp"
#include "operation.hpp"
#include "sequencer.hpp"
#include "stepper_motion_planner.hpp"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "timer_operations.hpp"
#include "w5500.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdbool.h>

constexpr std::uint8_t ECHO_SOCKET = 0;
constexpr std::uint16_t ECHO_PORT = 502;

static modbus_tcp_task_t modbus_task;

// Named rather than a temporary: StepperMotionPlanner stores the profile by
// reference, so a temporary would not bind and would dangle if it did.
static StepperMotionPlanner::SpeedProfile speed_profile_x{
    8000, // max_speed
    4000, // acceleration
    100,  // landing_speed
    2000, // homing_speed
};

static StepperMotionPlanner stepper_movement_x{
    speed_profile_x,
    hardware::stepper_driver_x,
};

static StepperMotionPlanner::SpeedProfile speed_profile_y{
    10000, // max_speed
    8000,  // acceleration
    100,   // landing_speed
    2000,  // homing_speed
};

static StepperMotionPlanner stepper_movement_y{
    speed_profile_y,
    hardware::stepper_driver_y,
};

static bool is_limit_switch_x_pressed = false;
static bool is_limit_switch_y_pressed = false;

// === Operations ===
static HomeStepperOperation home_x{stepper_movement_x, is_limit_switch_x_pressed, 20};
static HomeStepperOperation home_y{stepper_movement_y, is_limit_switch_y_pressed, 20};

static MoveStepperOperation move_x_input{stepper_movement_x, 0};
static MoveStepperOperation move_x_dip{stepper_movement_x, 8000};
static MoveStepperOperation move_x_output{stepper_movement_x, 16000};

static MoveStepperOperation move_y_up{stepper_movement_y, 0};
static MoveStepperOperation move_y_down{stepper_movement_y, 40000};

static EffectorOperation effector_engage{hardware::effector, true};
static EffectorOperation effector_disengage{hardware::effector, false};

static DwellOperation dwell{2000};

static std::array<Operation *, 13> operations = {
    // Home
    // &home_y,
    // &home_x,
    &move_y_up,
    &move_x_input,

    // Pick
    &move_y_down,
    &effector_engage,
    &move_y_up,

    // Dip
    &move_x_dip,
    &move_y_down,
    &dwell,
    &move_y_up,

    // Deposit
    &move_x_output,
    &move_y_down,

    // Release
    &effector_disengage,
    &move_y_up,
};

static Sequencer sequencer{std::span<Operation *>(operations)};

// === Modbus map ===

// Travel limits for the configurable positions, in steps.
// @todo: set from the measured axis travel
constexpr std::int32_t X_MAX_STEPS = 1000;
constexpr std::int32_t Y_MAX_STEPS = 500;

constexpr std::uint32_t DWELL_MAX_MS = 60000;

enum DiscreteInput : std::uint16_t
{
    DI_E_STOP_PRESSED,
    DI_START_PRESSED,
    DI_SEQUENCER_RUNNING,
    DI_X_AT_TARGET,
    DI_Y_AT_TARGET,
    DI_COUNT,
};

enum InputRegister : std::uint16_t
{
    IR_OPERATION_INDEX,
    IR_X_POSITION_HIGH,
    IR_X_POSITION_LOW,
    IR_Y_POSITION_HIGH,
    IR_Y_POSITION_LOW,
    IR_COUNT,
};

enum HoldingRegister : std::uint16_t
{
    HR_X_INPUT_POSITION,
    HR_X_DIP_POSITION,
    HR_X_OUTPUT_POSITION,
    HR_Y_UP_POSITION,
    HR_Y_DOWN_POSITION,
    HR_DWELL_TIME_MS,
    HR_COUNT,
};

static bool is_in_range(std::uint16_t start, std::uint16_t quantity, std::uint16_t count) noexcept
{
    return start < count && quantity <= count - start;
}

static bool is_pin_set(GPIO_TypeDef *port, std::uint16_t pin) noexcept
{
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}

static std::uint16_t high_word(std::int32_t value) noexcept
{
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) >> 16);
}

static std::uint16_t low_word(std::int32_t value) noexcept
{
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) & 0xFFFFU);
}

static bool read_discrete_input(std::uint16_t address) noexcept
{
    switch (address)
    {
    case DI_E_STOP_PRESSED:
        return is_pin_set(E_STOP_GPIO_Port, E_STOP_Pin);
    case DI_START_PRESSED:
        return is_pin_set(START_GPIO_Port, START_Pin);
    case DI_SEQUENCER_RUNNING:
        return sequencer.is_running();
    case DI_X_AT_TARGET:
        return stepper_movement_x.is_at_target();
    case DI_Y_AT_TARGET:
        return stepper_movement_y.is_at_target();
    default:
        return false; // Unreachable - callers check the range first
    }
}

static std::uint16_t read_input_register(std::uint16_t address) noexcept
{
    switch (address)
    {
    case IR_OPERATION_INDEX:
        return static_cast<std::uint16_t>(sequencer.operation_index());
    case IR_X_POSITION_HIGH:
        return high_word(stepper_movement_x.position_current_signed());
    case IR_X_POSITION_LOW:
        return low_word(stepper_movement_x.position_current_signed());
    case IR_Y_POSITION_HIGH:
        return high_word(stepper_movement_y.position_current_signed());
    case IR_Y_POSITION_LOW:
        return low_word(stepper_movement_y.position_current_signed());
    default:
        return 0; // Unreachable - callers check the range first
    }
}

static std::uint16_t read_holding_register(std::uint16_t address) noexcept
{
    switch (address)
    {
    case HR_X_INPUT_POSITION:
        return static_cast<std::uint16_t>(move_x_input.position());
    case HR_X_DIP_POSITION:
        return static_cast<std::uint16_t>(move_x_dip.position());
    case HR_X_OUTPUT_POSITION:
        return static_cast<std::uint16_t>(move_x_output.position());
    case HR_Y_UP_POSITION:
        return static_cast<std::uint16_t>(move_y_up.position());
    case HR_Y_DOWN_POSITION:
        return static_cast<std::uint16_t>(move_y_down.position());
    case HR_DWELL_TIME_MS:
        return static_cast<std::uint16_t>(dwell.dwell_time());
    default:
        return 0; // Unreachable - callers check the range first
    }
}

static bool is_valid_holding_value(std::uint16_t address, std::uint16_t value) noexcept
{
    switch (address)
    {
    case HR_X_INPUT_POSITION:
    case HR_X_DIP_POSITION:
    case HR_X_OUTPUT_POSITION:
        return value <= X_MAX_STEPS;
    case HR_Y_UP_POSITION:
    case HR_Y_DOWN_POSITION:
        return value <= Y_MAX_STEPS;
    case HR_DWELL_TIME_MS:
        return value <= DWELL_MAX_MS;
    default:
        return false;
    }
}

// Every value takes effect the next time its operation starts.
static void write_holding_register(std::uint16_t address, std::uint16_t value) noexcept
{
    switch (address)
    {
    case HR_X_INPUT_POSITION:
        move_x_input.set_position(value);
        break;
    case HR_X_DIP_POSITION:
        move_x_dip.set_position(value);
        break;
    case HR_X_OUTPUT_POSITION:
        move_x_output.set_position(value);
        break;
    case HR_Y_UP_POSITION:
        move_y_up.set_position(value);
        break;
    case HR_Y_DOWN_POSITION:
        move_y_down.set_position(value);
        break;
    case HR_DWELL_TIME_MS:
        dwell.set_dwell_time(value);
        break;
    default:
        break; // Unreachable - callers validate first
    }
}

static modbus_query_state_t modbus_read_discrete_inputs(std::uint16_t start, bool *dest,
                                                        std::uint16_t quantity)
{
    if (!is_in_range(start, quantity, DI_COUNT))
        return MODBUS_QUERY_ST_BAD_ADDRESS;

    for (std::uint16_t i = 0; i < quantity; i++)
        dest[i] = read_discrete_input(start + i);

    return MODBUS_QUERY_ST_OK;
}

static modbus_query_state_t modbus_read_input_registers(std::uint16_t start, std::uint16_t *dest,
                                                        std::uint16_t quantity)
{
    if (!is_in_range(start, quantity, IR_COUNT))
        return MODBUS_QUERY_ST_BAD_ADDRESS;

    for (std::uint16_t i = 0; i < quantity; i++)
        dest[i] = read_input_register(start + i);

    return MODBUS_QUERY_ST_OK;
}

static modbus_query_state_t modbus_read_holding_registers(std::uint16_t start, std::uint16_t *dest,
                                                          std::uint16_t quantity)
{
    if (!is_in_range(start, quantity, HR_COUNT))
        return MODBUS_QUERY_ST_BAD_ADDRESS;

    for (std::uint16_t i = 0; i < quantity; i++)
        dest[i] = read_holding_register(start + i);

    return MODBUS_QUERY_ST_OK;
}

static modbus_query_state_t modbus_write_holding_registers(std::uint16_t start,
                                                           const std::uint16_t *src,
                                                           std::uint16_t quantity)
{
    if (!is_in_range(start, quantity, HR_COUNT))
        return MODBUS_QUERY_ST_BAD_ADDRESS;

    // Validate everything first, so a rejected write changes nothing.
    // The library has no "illegal value" query state, so this reports a server failure.
    for (std::uint16_t i = 0; i < quantity; i++)
        if (!is_valid_holding_value(start + i, src[i]))
            return MODBUS_QUERY_ST_FAILURE;

    for (std::uint16_t i = 0; i < quantity; i++)
        write_holding_register(start + i, src[i]);

    return MODBUS_QUERY_ST_OK;
}

// Coils are left unmapped (illegal function): no remote commands yet.
static modbus_context_t modbus_ctx{
    .read_discrete_inputs = modbus_read_discrete_inputs,
    .read_coils = nullptr,
    .write_coils = nullptr,
    .read_input_registers = modbus_read_input_registers,
    .read_holding_registers = modbus_read_holding_registers,
    .write_holding_registers = modbus_write_holding_registers,
};

void setup()
{
    const uint32_t now = HAL_GetTick();

    // Initalize ethernet

    ethernet_W5500_init();

    uint8_t version = getVERSIONR();
    std::printf("W5500 version register: 0x%02X (expect 0x04)\r\n", version);

    modbus_tcp_task_init(&modbus_task, ECHO_SOCKET, ECHO_PORT, &modbus_ctx);

    // Initialize movement planners
    stepper_movement_x.initialize(now);
    stepper_movement_y.initialize(now);

    // Initialize safe defaults
    hardware::effector.engage();
}

void loop()
{

    // const bool is_e_stop_pressed = (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) ==
    // GPIO_PIN_RESET);
    const bool is_e_stop_pressed = false;
    const bool is_start_button_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
    // is_limit_switch_x_pressed = (HAL_GPIO_ReadPin(X_HOME_GPIO_Port, X_HOME_Pin) == GPIO_PIN_SET);
    is_limit_switch_x_pressed = false;
    // is_limit_switch_y_pressed = (HAL_GPIO_ReadPin(Y_HOME_GPIO_Port, Y_HOME_Pin) == GPIO_PIN_SET);
    is_limit_switch_y_pressed = false;

    if (is_limit_switch_x_pressed && stepper_movement_x.current_speed_signed() < 0)
    {
        stepper_movement_x.stop_immediately();
    }

    if (is_limit_switch_y_pressed && stepper_movement_y.current_speed_signed() < 0)
    {
        stepper_movement_y.stop_immediately();
    }

    if (is_e_stop_pressed)
    {
        sequencer.stop();
        stepper_movement_x.stop_immediately();
        stepper_movement_y.stop_immediately();
        hardware::effector.engage();
    }
    else
    {
        const uint32_t now = HAL_GetTick();

        if (is_start_button_pressed && !sequencer.is_running())
        {
            // Reset and restart the sequencer
            sequencer.restart();
        }

        // Sequencer step
        sequencer.sequencer_step();

        // Process the different modules
        stepper_movement_x.process(now);
        stepper_movement_y.process(now);
    }

    modbus_tcp_task_run(&modbus_task);
}
