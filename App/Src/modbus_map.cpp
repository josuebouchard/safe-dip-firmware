#include "modbus_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

// Travel limits for the configurable positions, in steps.
// @todo: set from the measured axis travel
constexpr std::int32_t X_MAX_STEPS = 1000;
constexpr std::int32_t Y_MAX_STEPS = 500;

constexpr std::int32_t DWELL_MAX_MS = 60000;

static Machine *machine_ = nullptr;

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

struct HoldingRegister
{
    std::int32_t Recipe::*field;
    std::int32_t max;
};

// The address is the row index, so a new register goes at the end.
static constexpr std::array HOLDING_REGISTERS = {
    HoldingRegister{&Recipe::x_input, X_MAX_STEPS},   // 0
    HoldingRegister{&Recipe::x_dip, X_MAX_STEPS},     // 1
    HoldingRegister{&Recipe::x_output, X_MAX_STEPS},  // 2
    HoldingRegister{&Recipe::y_up, Y_MAX_STEPS},      // 3
    HoldingRegister{&Recipe::y_down, Y_MAX_STEPS},    // 4
    HoldingRegister{&Recipe::dwell_ms, DWELL_MAX_MS}, // 5
};

constexpr std::uint16_t HR_COUNT = HOLDING_REGISTERS.size();

static bool is_in_range(std::uint16_t start, std::uint16_t quantity, std::uint16_t count) noexcept
{
    return start < count && quantity <= count - start;
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
        return machine_->inputs.e_stop_pressed;
    case DI_START_PRESSED:
        return machine_->inputs.start_pressed;
    case DI_SEQUENCER_RUNNING:
        return machine_->sequencer.is_running();
    case DI_X_AT_TARGET:
        return machine_->planner_x.is_at_target();
    case DI_Y_AT_TARGET:
        return machine_->planner_y.is_at_target();
    default:
        return false; // Unreachable - callers check the range first
    }
}

static std::uint16_t read_input_register(std::uint16_t address) noexcept
{
    switch (address)
    {
    case IR_OPERATION_INDEX:
        return static_cast<std::uint16_t>(machine_->sequencer.operation_index());
    case IR_X_POSITION_HIGH:
        return high_word(machine_->planner_x.position_current_signed());
    case IR_X_POSITION_LOW:
        return low_word(machine_->planner_x.position_current_signed());
    case IR_Y_POSITION_HIGH:
        return high_word(machine_->planner_y.position_current_signed());
    case IR_Y_POSITION_LOW:
        return low_word(machine_->planner_y.position_current_signed());
    default:
        return 0; // Unreachable - callers check the range first
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
    {
        const HoldingRegister &holding = HOLDING_REGISTERS[start + i];
        dest[i] = static_cast<std::uint16_t>(machine_->recipe.*holding.field);
    }

    return MODBUS_QUERY_ST_OK;
}

// Every value takes effect the next time its operation starts.
static modbus_query_state_t modbus_write_holding_registers(std::uint16_t start,
                                                           const std::uint16_t *src,
                                                           std::uint16_t quantity)
{
    if (!is_in_range(start, quantity, HR_COUNT))
        return MODBUS_QUERY_ST_BAD_ADDRESS;

    // Validate everything first, so a rejected write changes nothing.
    // The library has no "illegal value" query state, so this reports a server failure.
    for (std::uint16_t i = 0; i < quantity; i++)
        if (src[i] > HOLDING_REGISTERS[start + i].max)
            return MODBUS_QUERY_ST_FAILURE;

    for (std::uint16_t i = 0; i < quantity; i++)
    {
        const HoldingRegister &holding = HOLDING_REGISTERS[start + i];
        machine_->recipe.*holding.field = src[i];
    }

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

modbus_context_t *modbus_map_init(Machine &machine) noexcept
{
    machine_ = &machine;
    return &modbus_ctx;
}
