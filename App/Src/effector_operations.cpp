#include "effector_operations.hpp"
void EffectorEngageOperation::start() noexcept { effector_.engage(); }

void EffectorDisengageOperation::start() noexcept { effector_.disengage(); }
