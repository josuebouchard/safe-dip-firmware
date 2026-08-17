#ifndef SEQUENCER_INSTRUCTIONS_HPP
#define SEQUENCER_INSTRUCTIONS_HPP

enum class SequencerInstruction
{
    // Movement - X-axis
    move_x_home,
    move_x_dip,
    move_x_output,

    // Movement - Z-axis
    move_z_up,
    move_z_down,

    // Effector
    effector_engage,
    effector_disengage,

    // Timer
    timer_dwell
};

enum class SequencerInstructionState
{
    idle,
    running,
    finished,
    aborted
};

#endif // SEQUENCER_INSTRUCTIONS_HPP
