#ifndef LIMIT_FILTER_HPP
#define LIMIT_FILTER_HPP

#include <cstdint>

// Debounce only on press (a TON), output is the level.
//
// A press counts only once the switch has read pressed for press_ms without a break,
// so a noise spike from the stepper drivers can't fault a move. A release counts at once.
//
// The delay lets the axis run on past the switch by press_ms times its speed. Raise it
// if false hits get through, up to what that overrun allows.
class LimitFilter
{
  public:
    explicit constexpr LimitFilter(std::uint32_t press_ms) noexcept : press_ms_(press_ms) {}

    bool update(bool pressed, std::uint32_t now_ms) noexcept
    {
        if (pressed != last_pressed_)
        {
            last_pressed_ = pressed;
            changed_at_ms_ = now_ms;
        }

        return pressed && now_ms - changed_at_ms_ >= press_ms_;
    }

  private:
    std::uint32_t press_ms_;
    bool last_pressed_ = false;
    std::uint32_t changed_at_ms_ = 0;
};

#endif // LIMIT_FILTER_HPP
