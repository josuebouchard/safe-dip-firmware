#include "modbus_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::int32_t X_MAX_MM = Machine::X_TRAVEL_MM;
constexpr std::int32_t Y_MAX_MM = Machine::Y_TRAVEL_MM;
constexpr std::int32_t DWELL_MAX_MS = 60000;

// A holding register is 16 bits, so a larger limit could never be written.
static_assert(X_MAX_MM <= UINT16_MAX && Y_MAX_MM <= UINT16_MAX && DWELL_MAX_MS <= UINT16_MAX);

// The defaults must be values Modbus could write back.
static_assert(Recipe{}.x_input_mm <= X_MAX_MM && Recipe{}.x_dip_mm <= X_MAX_MM &&
              Recipe{}.x_output_mm <= X_MAX_MM);
static_assert(Recipe{}.y_up_mm <= Y_MAX_MM && Recipe{}.y_down_mm <= Y_MAX_MM);
static_assert(Recipe{}.dwell_ms <= DWELL_MAX_MS);

enum DiscreteInput : std::uint16_t
{
    DI_E_STOP_PRESSED,
    DI_START_PRESSED,
    DI_SEQUENCER_RUNNING,
    DI_X_AT_TARGET,
    DI_Y_AT_TARGET,
    DI_EFFECTOR_ENGAGED,
    DI_FAULT_ACTIVE,
    DI_COUNT,
};

enum InputRegister : std::uint16_t
{
    IR_OPERATION_INDEX = 0,
    IR_X_POSITION_HIGH = 1,
    IR_X_POSITION_LOW = 2,
    IR_Y_POSITION_HIGH = 3,
    IR_Y_POSITION_LOW = 4,
    IR_FAULTS_ACTIVE = 5,
    IR_FAULT_FIRST = 6,
    IR_COUNT,
};

struct HoldingRegister
{
    std::int32_t Recipe::*field;
    std::int32_t max;
};

// The address is the row index, so a new register goes at the end.
static constexpr std::array HOLDING_REGISTERS = {
    HoldingRegister{.field = &Recipe::x_input_mm, .max = X_MAX_MM},   // 0
    HoldingRegister{.field = &Recipe::x_dip_mm, .max = X_MAX_MM},     // 1
    HoldingRegister{.field = &Recipe::x_output_mm, .max = X_MAX_MM},  // 2
    HoldingRegister{.field = &Recipe::y_up_mm, .max = Y_MAX_MM},      // 3
    HoldingRegister{.field = &Recipe::y_down_mm, .max = Y_MAX_MM},    // 4
    HoldingRegister{.field = &Recipe::dwell_ms, .max = DWELL_MAX_MS}, // 5
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

static bool read_discrete_input(const Machine &machine, std::uint16_t address) noexcept
{
    switch (address)
    {
    case DI_E_STOP_PRESSED:
        return machine.inputs.e_stop_pressed;
    case DI_START_PRESSED:
        return machine.inputs.start_pressed;
    case DI_SEQUENCER_RUNNING:
        return machine.sequencer.is_running();
    case DI_X_AT_TARGET:
        return machine.controller_x.is_at_target();
    case DI_Y_AT_TARGET:
        return machine.controller_y.is_at_target();
    case DI_EFFECTOR_ENGAGED:
        return hardware::effector.is_engaged();
    case DI_FAULT_ACTIVE:
        return machine.faults.any();
    default:
        return false; // Unreachable - callers check the range first
    }
}

static std::uint16_t read_input_register(const Machine &machine, std::uint16_t address) noexcept
{
    switch (address)
    {
    case IR_OPERATION_INDEX:
        return static_cast<std::uint16_t>(machine.sequencer.operation_index());
    case IR_X_POSITION_HIGH:
        return high_word(machine.controller_x.position_current_signed());
    case IR_X_POSITION_LOW:
        return low_word(machine.controller_x.position_current_signed());
    case IR_Y_POSITION_HIGH:
        return high_word(machine.controller_y.position_current_signed());
    case IR_Y_POSITION_LOW:
        return low_word(machine.controller_y.position_current_signed());
    case IR_FAULTS_ACTIVE:
        return machine.faults.active();
    case IR_FAULT_FIRST:
        return machine.faults.first();
    default:
        return 0; // Unreachable - callers check the range first
    }
}

static modbus_query_state_t modbus_read_discrete_inputs(std::uint16_t start, bool *dest,
                                                        std::uint16_t quantity, void *user_context)
{
    const Machine &machine = *static_cast<const Machine *>(user_context);

    if (!is_in_range(start, quantity, DI_COUNT))
    {
        return MODBUS_QUERY_ST_BAD_ADDRESS;
    }

    for (std::uint16_t i = 0; i < quantity; i++)
    {
        dest[i] = read_discrete_input(machine, start + i);
    }

    return MODBUS_QUERY_ST_OK;
}

static modbus_query_state_t modbus_read_input_registers(std::uint16_t start, std::uint16_t *dest,
                                                        std::uint16_t quantity, void *user_context)
{
    const Machine &machine = *static_cast<const Machine *>(user_context);

    if (!is_in_range(start, quantity, IR_COUNT))
    {
        return MODBUS_QUERY_ST_BAD_ADDRESS;
    }

    for (std::uint16_t i = 0; i < quantity; i++)
    {
        dest[i] = read_input_register(machine, start + i);
    }

    return MODBUS_QUERY_ST_OK;
}

static modbus_query_state_t modbus_read_holding_registers(std::uint16_t start, std::uint16_t *dest,
                                                          std::uint16_t quantity,
                                                          void *user_context)
{
    const Machine &machine = *static_cast<const Machine *>(user_context);

    if (!is_in_range(start, quantity, HR_COUNT))
    {
        return MODBUS_QUERY_ST_BAD_ADDRESS;
    }

    for (std::uint16_t i = 0; i < quantity; i++)
    {
        const HoldingRegister &holding = HOLDING_REGISTERS[start + i];
        dest[i] = static_cast<std::uint16_t>(machine.recipe.*holding.field);
    }

    return MODBUS_QUERY_ST_OK;
}

// Every value takes effect the next time its operation starts.
static modbus_query_state_t modbus_write_holding_registers(std::uint16_t start,
                                                           const std::uint16_t *src,
                                                           std::uint16_t quantity,
                                                           void *user_context)
{
    Machine &machine = *static_cast<Machine *>(user_context);

    if (!is_in_range(start, quantity, HR_COUNT))
    {
        return MODBUS_QUERY_ST_BAD_ADDRESS;
    }

    // Validate everything first, so a rejected write changes nothing.
    // The library has no "illegal value" query state, so this reports a server failure.
    for (std::uint16_t i = 0; i < quantity; i++)
    {
        if (src[i] > HOLDING_REGISTERS[start + i].max)
        {
            return MODBUS_QUERY_ST_FAILURE;
        }
    }

    for (std::uint16_t i = 0; i < quantity; i++)
    {
        const HoldingRegister &holding = HOLDING_REGISTERS[start + i];
        machine.recipe.*holding.field = src[i];
    }

    return MODBUS_QUERY_ST_OK;
}

// Coils are left unmapped (illegal function): no remote commands yet.
modbus_context_t modbus_map_make_context(Machine &machine) noexcept
{
    return modbus_context_t{
        .read_discrete_inputs = modbus_read_discrete_inputs,
        .read_coils = nullptr,
        .write_coils = nullptr,
        .read_input_registers = modbus_read_input_registers,
        .read_holding_registers = modbus_read_holding_registers,
        .write_holding_registers = modbus_write_holding_registers,
        .user_context = &machine,
    };
}
