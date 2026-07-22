
#ifndef MODBUS_ASSERT_H
#define MODBUS_ASSERT_H

#include <stdint.h>

typedef struct
{
    const char *expression;
    const char *file;
    const char *function;
    uint32_t line;
} modbus_assert_info_t;

/*
 * Supplied by the final application.
 * This function must never return.
 */
_Noreturn void modbus_assertion_failed(const modbus_assert_info_t *info);

#endif // MODBUS_ASSERT_H
