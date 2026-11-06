#ifndef FAULTS_HPP
#define FAULTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

// The order is the Modbus format: a fault's value is its bit number and its source
// register offset, so a new fault goes right above `count`. See docs/modbus-map.md.
enum class Fault : std::uint8_t
{
    e_stop,
    operation_interrupted,
    operation_timed_out,
    operation_failed,
    home_switch_stuck,
    position_drift,
    unexpected_reset,
    count, // Not a fault: the number of faults above. Never raise it.
};

static_assert(static_cast<std::size_t>(Fault::count) <= 16, "active_ has 16 bits");

constexpr std::uint16_t NO_SOURCE = 0xFFFF;

// Which faults have happened since the last clear, and where each one came from.
class Faults
{
  public:
    void raise(Fault fault, std::uint16_t source = NO_SOURCE) noexcept
    {
        const auto index = static_cast<std::size_t>(fault);
        const auto bit = static_cast<std::uint16_t>(1U << index);

        if ((active_ & bit) != 0)
        {
            return; // Already latched, so keep the original source
        }

        if (active_ == 0)
        {
            first_ = bit;
        }

        active_ |= bit;
        sources_[index] = source;
    }

    void clear() noexcept
    {
        active_ = 0;
        first_ = 0;
    }

    bool any() const noexcept { return active_ != 0; }
    std::uint16_t active() const noexcept { return active_; }
    std::uint16_t first() const noexcept { return first_; } // As a bit, like active()

    std::uint16_t source(std::size_t index) const noexcept
    {
        const bool is_latched = ((active_ >> index) & 1U) != 0U;
        return is_latched ? sources_[index] : NO_SOURCE;
    }

  private:
    std::uint16_t active_ = 0;
    std::uint16_t first_ = 0;
    std::array<std::uint16_t, static_cast<std::size_t>(Fault::count)> sources_{};
};

#endif // FAULTS_HPP
