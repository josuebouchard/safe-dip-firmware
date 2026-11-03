#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include "sequencer.hpp"

struct Machine;

// Builds the operations and the cycle on first call. The members of `machine`
// that the operations use must already be constructed.
Sequencer &make_sequence(Machine &machine);

#endif // SEQUENCE_HPP
