#include "effector_operations.hpp"

void EffectorOperation::start() noexcept
{
    should_engage_ ? effector_.engage() : effector_.disengage();
}