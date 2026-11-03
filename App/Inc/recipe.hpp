#ifndef RECIPE_HPP
#define RECIPE_HPP

#include <cstdint>

// Values tunable at runtime. Operations read them, Modbus writes them.
struct Recipe
{
    // Steps
    std::int32_t x_input = 0;
    std::int32_t x_dip = 8000;
    std::int32_t x_output = 16000;
    std::int32_t y_up = 0;
    std::int32_t y_down = 40000;

    std::int32_t dwell_ms = 2000;
};

#endif // RECIPE_HPP
