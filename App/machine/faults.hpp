#ifndef FAULTS_HPP
#define FAULTS_HPP

#include <cstddef>
#include <cstdint>

// The order is the Modbus format: a fault's value is its bit number, so a new fault
// goes right above `count`. See docs/modbus-map.md.
enum class Fault : std::uint8_t
{
    e_stop,
    operation_interrupted,
    operation_timed_out,
    operation_failed,
    x_homing_failed,
    y_homing_failed,
    count, // Not a fault: the number of faults above. Never raise it.
};

static_assert(static_cast<std::size_t>(Fault::count) <= 16, "active_ has 16 bits");

// Which faults have happened since the last clear.
class Faults
{
  public:
    void raise(Fault fault) noexcept
    {
        const auto bit = static_cast<std::uint16_t>(1U << static_cast<std::size_t>(fault));

        if (active_ == 0)
        {
            first_ = bit;
        }

        active_ |= bit;
    }

    void clear() noexcept
    {
        active_ = 0;
        first_ = 0;
    }

    [[nodiscard]] bool any() const noexcept { return active_ != 0; }
    [[nodiscard]] std::uint16_t active() const noexcept { return active_; }
    [[nodiscard]] std::uint16_t first() const noexcept { return first_; } // As a bit, like active()

  private:
    std::uint16_t active_ = 0;
    std::uint16_t first_ = 0;
};

#endif // FAULTS_HPP
