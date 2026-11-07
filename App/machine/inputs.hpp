#ifndef INPUTS_HPP
#define INPUTS_HPP

// Input levels for one loop pass. They are meant to be read once at the
// top of loop(), so everything in that pass acts on the same values.
struct Inputs
{
    bool e_stop_pressed = false;
    bool start_pressed = false;
    bool limit_x_pressed = false;
    bool limit_y_pressed = false;
};

#endif // INPUTS_HPP
