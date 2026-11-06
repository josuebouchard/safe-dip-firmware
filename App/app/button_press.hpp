#ifndef BUTTON_PRESS_HPP
#define BUTTON_PRESS_HPP

#include <cstdint>

// Turns a raw button level into one event per press.
// Only triggers the event on debounced rising-edge.
class ButtonPress
{
  public:
    bool update(bool level, std::uint32_t now_ms) noexcept
    {
        if (level != last_level_)
        {
            last_level_ = level;
            changed_at_ms_ = now_ms;
        }

        if (level == stable_level_ || now_ms - changed_at_ms_ < DEBOUNCE_MS)
        {
            return false;
        }

        stable_level_ = level;
        return stable_level_;
    }

  private:
    static constexpr std::uint32_t DEBOUNCE_MS = 20;

    bool last_level_ = false;
    bool stable_level_ = false;
    std::uint32_t changed_at_ms_ = 0;
};

#endif // BUTTON_PRESS_HPP
