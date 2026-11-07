#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include "sequencer.hpp"
#include "tick.hpp"

// Sequencer<Tick> is compiled once, in sequencer_instance.cpp. This tells every file
// that includes this header to use that copy, and not emit its own for the members
// defined in the class.
extern template class Sequencer<Tick>;

struct Machine;

// Builds the operations and the cycle on first call. The members of `machine`
// that the operations use must already be constructed.
Sequencer<Tick> &make_sequence(Machine &machine);

#endif // SEQUENCE_HPP
