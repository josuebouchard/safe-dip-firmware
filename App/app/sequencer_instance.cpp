// The one place Sequencer<Tick> is compiled. sequencer_impl.hpp holds the definitions
// and is included nowhere else; every other file sees only the declarations in
// sequencer.hpp, and sequence.hpp tells them not to instantiate it themselves.

#include "sequencer_impl.hpp"
#include "tick.hpp"

template class Sequencer<Tick>;
