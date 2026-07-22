#ifndef MODBUS_ASSERT_INTERNAL_H
#define MODBUS_ASSERT_INTERNAL_H

#include "modbus_assert.h"

#define MODBUS_ASSERT(condition)                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            const modbus_assert_info_t modbus_assert_info_ = {                                     \
                .expression = #condition,                                                          \
                .file = __FILE__,                                                                  \
                .function = __func__,                                                              \
                .line = (uint32_t)__LINE__,                                                        \
            };                                                                                     \
                                                                                                   \
            modbus_assertion_failed(&modbus_assert_info_);                                         \
        }                                                                                          \
    } while (0)

#endif // MODBUS_ASSERT_INTERNAL_H
