#ifndef RECIPE_HPP
#define RECIPE_HPP

#include <cstdint>

// Values tunable at runtime. Operations read them, Modbus writes them.
struct Recipe
{
    // mm from the origin set by homing
    std::int32_t x_input_mm = 0;
    std::int32_t x_dip_mm = 250;
    std::int32_t x_output_mm = 500;
    std::int32_t y_up_mm = 0;
    std::int32_t y_down_mm = 100;

    std::int32_t dwell_ms = 2000;
};

#endif // RECIPE_HPP
