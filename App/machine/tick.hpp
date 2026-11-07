#ifndef TICK_HPP
#define TICK_HPP

#include "faults.hpp"
#include "inputs.hpp"

#include <cstdint>

// What every operation receives on each loop pass: the values that change from one
// pass to the next. Anything fixed for the life of the program (which controller,
// which switch, which fault to raise) is bound through the constructor instead.
//
// loop() builds one per pass, so every operation in that pass sees the same time
// and the same input snapshot.
struct Tick
{
    std::uint32_t now_ms;
    const Inputs &inputs;
    Faults &faults; // Operations raise here. Only loop() clears.
};

#endif // TICK_HPP
